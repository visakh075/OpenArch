#include "DbManagerSQLite.h"
#include <sstream>

static const char* SCHEMA = R"sql(
CREATE TABLE IF NOT EXISTS nodes(
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 name TEXT, type TEXT, metadata TEXT, attributes TEXT);
CREATE TABLE IF NOT EXISTS layers(
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 name TEXT, kind TEXT, metadata TEXT, attributes TEXT);
CREATE TABLE IF NOT EXISTS edges(
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 src_node_id INTEGER, dst_node_id INTEGER,
 edge_type TEXT, metadata TEXT, attributes TEXT);
)sql";

std::string DbManagerSQLite::toJson(const std::unordered_map<std::string,std::string>& m){
    std::ostringstream o; o<<"{";
    bool f=true;
    for(auto&[k,v]:m){ if(!f) o<<","; f=false; o<<"\""<<k<<"\":\""<<v<<"\""; }
    o<<"}"; return o.str();
}

DbManagerSQLite::DbManagerSQLite()=default;
DbManagerSQLite::~DbManagerSQLite(){ close(); }

Result DbManagerSQLite::open(const std::string& p){
 if(sqlite3_open(p.c_str(), &db_)!=SQLITE_OK)
  return Result::failure("Failed to open DB");
 return exec(SCHEMA);
}

void DbManagerSQLite::close(){ if(db_) sqlite3_close(db_), db_=nullptr; }

Result DbManagerSQLite::exec(const char* s){
 char* e=nullptr;
 if(sqlite3_exec(db_, s, nullptr, nullptr, &e)!=SQLITE_OK){
  std::string m=e?e:"sql error"; sqlite3_free(e);
  return Result::failure(m);
 }
 return Result::success();
}

#define SIMPLE_CREATE(sql,bind,lastid)  sqlite3_stmt* st;  if(sqlite3_prepare_v2(db_,sql,-1,&st,nullptr)!=SQLITE_OK) return Result::failure("prepare failed");  bind;  if(sqlite3_step(st)!=SQLITE_DONE){ sqlite3_finalize(st); return Result::failure("execute failed"); }  sqlite3_finalize(st); lastid = sqlite3_last_insert_rowid(db_);  return Result::success();

#define SIMPLE_UPDATE(sql,bind)  sqlite3_stmt* st;  if(sqlite3_prepare_v2(db_,sql,-1,&st,nullptr)!=SQLITE_OK) return Result::failure("prepare failed");  bind;  if(sqlite3_step(st)!=SQLITE_DONE){ sqlite3_finalize(st); return Result::failure("execute failed"); }  sqlite3_finalize(st);  return Result::success();

Result DbManagerSQLite::createNode(const NodeData& n, NodeId& id){
 SIMPLE_CREATE(
  "INSERT INTO nodes(name,type,metadata,attributes)VALUES(?,?,?,?)",
  sqlite3_bind_text(st,1,n.name.c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,2,n.type.c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,3,toJson(n.metadata).c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,4,toJson(n.attributes).c_str(),-1,SQLITE_TRANSIENT);,
  id
 )
}

Result DbManagerSQLite::updateNode(const NodeData& n){
 SIMPLE_UPDATE(
  "UPDATE nodes SET name=?,type=?,metadata=?,attributes=? WHERE id=?",
  sqlite3_bind_text(st,1,n.name.c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,2,n.type.c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,3,toJson(n.metadata).c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,4,toJson(n.attributes).c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_int64(st,5,n.id);
 )
}

Result DbManagerSQLite::deleteNode(NodeId i){
 SIMPLE_UPDATE(
  "DELETE FROM nodes WHERE id=?",
  sqlite3_bind_int64(st,1,i);
 )
}

std::vector<NodeData> DbManagerSQLite::getAllNodes(){
 std::vector<NodeData> o; sqlite3_stmt* st;
 sqlite3_prepare_v2(db_,"SELECT id,name,type FROM nodes",-1,&st,nullptr);
 while(sqlite3_step(st)==SQLITE_ROW){
  NodeData n; n.id=sqlite3_column_int64(st,0);
  n.name=(const char*)sqlite3_column_text(st,1);
  n.type=(const char*)sqlite3_column_text(st,2); o.push_back(n);
 }
 sqlite3_finalize(st); return o;
}

Result DbManagerSQLite::createLayer(const LayerData& l, LayerId& id){
 SIMPLE_CREATE(
  "INSERT INTO layers(name,kind,metadata,attributes)VALUES(?,?,?,?)",
  sqlite3_bind_text(st,1,l.name.c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,2,l.kind.c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,3,toJson(l.metadata).c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,4,toJson(l.attributes).c_str(),-1,SQLITE_TRANSIENT);,
  id
 )
}

Result DbManagerSQLite::updateLayer(const LayerData& l){
 SIMPLE_UPDATE(
  "UPDATE layers SET name=?,kind=?,metadata=?,attributes=? WHERE id=?",
  sqlite3_bind_text(st,1,l.name.c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,2,l.kind.c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,3,toJson(l.metadata).c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,4,toJson(l.attributes).c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_int64(st,5,l.id);
 )
}

Result DbManagerSQLite::deleteLayer(LayerId i){
 SIMPLE_UPDATE(
  "DELETE FROM layers WHERE id=?",
  sqlite3_bind_int64(st,1,i);
 )
}

std::vector<LayerData> DbManagerSQLite::getAllLayers(){
 std::vector<LayerData> o; sqlite3_stmt* st;
 sqlite3_prepare_v2(db_,"SELECT id,name,kind FROM layers",-1,&st,nullptr);
 while(sqlite3_step(st)==SQLITE_ROW){
  LayerData l; l.id=sqlite3_column_int64(st,0);
  l.name=(const char*)sqlite3_column_text(st,1);
  l.kind=(const char*)sqlite3_column_text(st,2); o.push_back(l);
 }
 sqlite3_finalize(st); return o;
}

Result DbManagerSQLite::createEdge(const EdgeData& e, EdgeId& id){
 SIMPLE_CREATE(
  "INSERT INTO edges(src_node_id,dst_node_id,edge_type,metadata,attributes)VALUES(?,?,?,?,?)",
  sqlite3_bind_int64(st,1,e.srcNode);
  sqlite3_bind_int64(st,2,e.dstNode);
  sqlite3_bind_text(st,3,e.edgeType.c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,4,toJson(e.metadata).c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,5,toJson(e.attributes).c_str(),-1,SQLITE_TRANSIENT);,
  id
 )
}

Result DbManagerSQLite::updateEdge(const EdgeData& e){
 SIMPLE_UPDATE(
  "UPDATE edges SET edge_type=?,metadata=?,attributes=? WHERE id=?",
  sqlite3_bind_text(st,1,e.edgeType.c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,2,toJson(e.metadata).c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,3,toJson(e.attributes).c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_int64(st,4,e.id);
 )
}

Result DbManagerSQLite::deleteEdge(EdgeId i){
 SIMPLE_UPDATE(
  "DELETE FROM edges WHERE id=?",
  sqlite3_bind_int64(st,1,i);
 )
}

std::vector<EdgeData> DbManagerSQLite::getAllEdges(){
 std::vector<EdgeData> o; sqlite3_stmt* st;
 sqlite3_prepare_v2(db_,"SELECT id,src_node_id,dst_node_id,edge_type FROM edges",-1,&st,nullptr);
 while(sqlite3_step(st)==SQLITE_ROW){
  EdgeData e; e.id=sqlite3_column_int64(st,0);
  e.srcNode=sqlite3_column_int64(st,1);
  e.dstNode=sqlite3_column_int64(st,2);
  e.edgeType=(const char*)sqlite3_column_text(st,3); o.push_back(e);
 }
 sqlite3_finalize(st); return o;
}
