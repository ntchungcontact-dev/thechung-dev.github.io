#include <iostream>
#include <vector>
#include <map>
#include <string>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;
using namespace httplib;

// DATA
vector<string> users = {"userA","userB","admin"};
vector<string> files = {"file1","file2","file3"};

// quyền: 1=R, 2=W, 4=X
vector<vector<int>> perm = {
    {1,3,0},
    {1,1,1},
    {7,7,7}
};

struct Log{
    string user;
    string action;
    bool ok;
};

vector<Log> logs;
map<string,int> sessions;
map<string,int> denyCount;
map<string,bool> blocked;

// SERVER
int main(){
    Server svr;

    // serve web
    svr.set_mount_point("/", "./public");

    // GET matrix
    svr.Get("/api/matrix",[](const Request&, Response& res){
        res.set_content(json({
            {"users",users},
            {"files",files},
            {"perm",perm}
        }).dump(),"application/json");
    });

    // SAVE matrix
    svr.Post("/api/matrix",[](const Request& req, Response& res){
        auto j = json::parse(req.body);
        perm = j["perm"].get<vector<vector<int>>>();
        res.set_content("{\"ok\":true}","application/json");
    });

    // LOGIN
    svr.Post("/api/login",[](const Request& req, Response& res){
        auto j = json::parse(req.body);
        string user = j["user"];

        int uid = -1;
        for(int i=0;i<users.size();i++){
            if(users[i]==user) uid=i;
        }

        string token = user + "_token";
        sessions[token] = uid;

        res.set_content(json({
            {"ok",true},
            {"user_id",uid},
            {"token",token}
        }).dump(),"application/json");
    });

    // ACTION
    svr.Post("/api/action",[](const Request& req, Response& res){
        auto j = json::parse(req.body);

        int uid = j["user_id"];
        int fid = j["file_id"];
        string action = j["action"];

        string user = users[uid];

        // check blocked
        if(blocked[user]){
            res.set_content(json({
                {"allowed",false},
                {"blocked",true}
            }).dump(),"application/json");
            return;
        }

        int need = (action=="open"?1:(action=="edit"?2:2));
        int have = perm[uid][fid];
        bool ok = (have & need) == need;

        // log
        logs.push_back({user,action,ok});

        // detect spam
        if(!ok){
            denyCount[user]++;
            if(denyCount[user] >= 5){
                blocked[user] = true;
            }
        }

        res.set_content(json({
            {"allowed",ok}
        }).dump(),"application/json");
    });

    // GET LOGS
    svr.Get("/api/logs",[](const Request&, Response& res){
        json arr = json::array();
        for(auto &l: logs){
            arr.push_back({
                {"user",l.user},
                {"action",l.action},
                {"ok",l.ok}
            });
        }
        res.set_content(arr.dump(),"application/json");
    });

    // REALTIME STREAM (SSE)
    svr.Get("/api/stream",[](const Request&, Response& res){
        res.set_header("Content-Type","text/event-stream");
        res.set_header("Cache-Control","no-cache");

        string data="";
        for(auto &l: logs){
            data += "data: " + json({
                {"user",l.user},
                {"action",l.action},
                {"ok",l.ok}
            }).dump() + "\n\n";
        }

        res.set_content(data,"text/event-stream");
    });

    cout << "Server running at http://localhost:8080\n";
    svr.listen("0.0.0.0",8080);
}