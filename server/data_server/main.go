package main

import (
	"context"
	"database/sql"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net/http"
	"time"

	_ "github.com/go-sql-driver/mysql"
	"github.com/golang/glog"
	"github.com/gorilla/websocket"
	"github.com/juju/errors"
	"github.com/rojer/mgrpc"
	mgws "github.com/rojer/mgrpc/websocket"
)

var listenAddr = flag.String("listen-addr", "127.0.0.1:8910", "http service address")
var sqlConn = flag.String("sql-conn", "", "SQL server connect string")

type SensorData struct {
	SID       int     `json:"sid"`
	SubID     int     `json:"subid"`
	Timestamp float64 `json:"ts"`
	Value     float64 `json:"v"`
}

func addDataHandler(peer string, params json.RawMessage, db *sql.DB) {
	var sd SensorData
	if err := json.Unmarshal(params, &sd); err != nil {
		glog.Errorf("%s: invalid sensor data: %s", peer, params)
		return
	}
	processSensorData(peer, db, &sd)
}

type ReportTemp struct {
	SID       int      `json:"sid"`
	SubID     int      `json:"subid"`
	Timestamp float64  `json:"ts"`
	Temp      *float64 `json:"temp,omitempty"`
	RH        *float64 `json:"rh,omitempty"`
}

func reportTempHandler(peer string, params json.RawMessage, db *sql.DB) {
	var rt ReportTemp
	if err := json.Unmarshal(params, &rt); err != nil {
		glog.Errorf("%s: invalid report temp data: %s", peer, params)
		return
	}
	if rt.Temp != nil {
		processSensorData(peer, db, &SensorData{
			SID:       rt.SID,
			SubID:     0,
			Timestamp: rt.Timestamp,
			Value:     *rt.Temp,
		})
	}
	if rt.RH != nil {
		processSensorData(peer, db, &SensorData{
			SID:       rt.SID,
			SubID:     1,
			Timestamp: rt.Timestamp,
			Value:     *rt.RH,
		})
	}
}

func processSensorData(peer string, db *sql.DB, sd *SensorData) {
	glog.Infof("%s: sensor data: %#v", peer, sd)
	var err error
	if sd.Timestamp > 1500000000 {
		ts := time.Unix(int64(sd.Timestamp), int64(sd.Timestamp*float64(time.Nanosecond)))
		st, err := db.Prepare("INSERT INTO data SET sid = ?, subid = ?, ts = ?, value = ?")
		if err != nil {
			glog.Errorf("failed to prepare statement: %s", err)
			return
		}
		defer st.Close()
		_, err = st.Exec(sd.SID, sd.SubID, ts, sd.Value)
	} else {
		st, _ := db.Prepare("INSERT INTO data SET sid = ?, subid = ?, value = ?")
		if err != nil {
			glog.Errorf("failed to prepare statement: %s", err)
			return
		}
		defer st.Close()
		_, err = st.Exec(sd.SID, sd.SubID, sd.Value)
	}
}

type SensorGetDataRequest struct {
	SID           int     `json:"sid"`
	SubID         int     `json:"subid"`
	TimestampFrom float64 `json:"ts_from"`
	TimestampTo   float64 `json:"ts_to"`
	Limit         int     `json:"limit"`
}

type SensorDataEntry struct {
	Timestamp float64 `json:"ts"`
	Value     float64 `json:"v"`
}

type SensorGetDataResponse struct {
	Data []*SensorDataEntry `json:"data"`
}

func sensorGetDataHandler(peer string, params json.RawMessage, db *sql.DB) (interface{}, error) {
	var err error
	var req SensorGetDataRequest
	if err = json.Unmarshal(params, &req); err != nil {
		glog.Errorf("%s: invalid request", peer, params)
		return nil, errors.Errorf("%s: invalid request", peer, params)
	}
	if req.Limit == 0 {
		req.Limit = 100
	}
	var rows *sql.Rows
	if req.TimestampFrom > 0 {
		tsFrom := time.Unix(int64(req.TimestampFrom), int64(req.TimestampFrom*float64(time.Nanosecond)))
		tsTo := time.Unix(int64(req.TimestampTo), int64(req.TimestampTo*float64(time.Nanosecond)))
		glog.Infof("%s %s", tsFrom, tsTo)
		rows, err = db.Query(
			"SELECT ts, value FROM sensor_data.data "+
				"WHERE sid = ? AND subid = ? AND ts >= ? AND ts < ? "+
				"ORDER BY ts LIMIT ?",
			req.SID, req.SubID, tsFrom, tsTo, req.Limit)
	} else {
		rows, err = db.Query(
			"SELECT ts, value FROM sensor_data.data "+
				"WHERE sid = ? AND subid = ? "+
				"ORDER BY ts LIMIT ?",
			req.SID, req.SubID, req.Limit)
	}
	if err != nil {
		glog.Errorf("%s: query error %#v: %s", peer, req, err)
		return nil, errors.Errorf("%s: query error %#v: %s", peer, req, err)
	}
	defer rows.Close()
	var resp SensorGetDataResponse
	for rows.Next() {
		var ts time.Time
		var value float64
		if err := rows.Scan(&ts, &value); err != nil {
			glog.Errorf("%s: scan error %#v: %s", peer, params, err)
			return nil, errors.Errorf("%s: query error %#v: %s", peer, req, err)
		}
		tsFloat := float64(ts.UnixNano()) / 1000000000.0
		resp.Data = append(resp.Data, &SensorDataEntry{Timestamp: tsFloat, Value: value})
	}
	glog.Infof("%s: request: %#v, data: %d points", peer, req, len(resp.Data))
	return &resp, nil
}

type mgRPCConnHandler struct {
	db   *sql.DB
	peer string
}

func (mch *mgRPCConnHandler) Handle(ctx context.Context, jsc *mgrpc.Conn, req *mgrpc.Request) {
	glog.V(1).Infof("%#v", req)
	var resp interface{}
	var rerr *mgrpc.Error
	var err error
	switch req.Method {
	case "Sensor.ReportTemp":
		reportTempHandler(mch.peer, *req.Params, mch.db)
	case "MyHome.Data.Add":
		fallthrough
	case "Sensor.Data": // Old name
		addDataHandler(mch.peer, *req.Params, mch.db)
	case "Sensor.GetData":
		resp, err = sensorGetDataHandler(mch.peer, *req.Params, mch.db)
	default:
		rerr = &mgrpc.Error{
			Code:    mgrpc.CodeMethodNotFound,
			Message: "Method not found",
		}
	}
	switch {
	case err == nil && rerr == nil:
		jsc.Reply(ctx, req.ID, resp)
	case rerr != nil:
		jsc.ReplyWithError(ctx, req.ID, rerr)
	case err != nil:
		jsc.ReplyWithError(ctx, req.ID, &mgrpc.Error{
			Code:    -1,
			Message: err.Error(),
		})
	}
}

func rootHTTPHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Fprintf(w, "Nothing to see here, please move along.\r\n")
}

func rpcHTTPHandler(w http.ResponseWriter, r *http.Request) {
	var upgrader = websocket.Upgrader{
		CheckOrigin: func(r *http.Request) bool {
			return true // Everyone is welcome
		},
	}
	wsc, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		glog.Errorf("upgrade: %s", err)
		return
	}
	defer wsc.Close()
	mch := &mgRPCConnHandler{peer: wsc.RemoteAddr().String()}
	if rip := r.Header["X-Real-Ip"]; rip != nil {
		mch.peer += fmt.Sprintf(" (%s)", rip[0])
	}
	glog.Infof("%s: Connected, headers: %s", mch.peer, r.Header)
	jsrpc := mgrpc.NewConn(context.Background(), mgws.NewObjectStream(wsc), mgrpc.AsyncHandler(mch), nil)
	defer jsrpc.Close()
	db, err := sql.Open("mysql", *sqlConn)
	if err != nil {
		glog.Errorf("error connecting to db: %s", err)
		return
	}
	defer db.Close()
	mch.db = db
	<-jsrpc.DisconnectNotify()
	glog.Infof("%s: Disconnected", mch.peer)
}

func main() {
	flag.Parse()
	if *sqlConn == "" {
		glog.Fatalf("--sql-conn not set")
	}
	http.HandleFunc("/rpc", rpcHTTPHandler)
	http.HandleFunc("/", rootHTTPHandler)
	glog.Infof("Serving at %s", *listenAddr)
	err := http.ListenAndServe(*listenAddr, nil)
	if err != nil {
		log.Fatal("ListenAndServe: ", err)
	}
}
