#include <ctime>
#include <map>
#include <regex>
#include <set>
#include <signal.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

// Expire a client if not being pingged by it in 5 seconds.
const int CLIENT_EXPIRY = 5;

// Broadcast at most 4 latest alarms to all clients.
const int MAX_ALARMS = 4;

// Listen to port 37813
const int PORT = 37813;

typedef websocketpp::server<websocketpp::config::asio> server;

using namespace std;
using websocketpp::connection_hdl;

namespace {

map<string, pair<connection_hdl, time_t> > alarms;
mutex alarms_lock;

map<connection_hdl, time_t, owner_less<connection_hdl> > clients;
mutex clients_lock;

server s;

}

string get_time_string(const time_t& t) {
  string s(ctime(&t));
  return regex_replace(s.substr(0, s.rfind(" ")), regex("  +"), " ");
}

bool time_cmp_desc(pair<string, time_t>& a, pair<string, time_t>& b) {
    return a.second > b.second;
}

string get_alarms() {
  vector<pair<string, time_t> > v;

  {
    lock_guard<mutex> guard(alarms_lock);
    for (auto it = alarms.begin(); it != alarms.end(); it++) {
      v.push_back(make_pair(it->first, it->second.second));
    }
  }

  sort(v.begin(), v.end(), time_cmp_desc);

  int i = 0;
  string result;
  for (auto it = v.begin(); it != v.end() && i < MAX_ALARMS; it++, i++) {
    if (i > 0) {
      result += "\n";
    }
    result += it->first + "\n  " + get_time_string(it->second);
  }
  return result;
}

void remove_alarms(connection_hdl hdl) {
  lock_guard<mutex> guard(alarms_lock);
  owner_less<connection_hdl> comp;
  for (auto it = alarms.begin(), next = it; it != alarms.end(); it = next) {
    next++;
    if (!comp(it->second.first, hdl) && !comp(hdl, it->second.first)) {
      alarms.erase(it);
    }
  }
}

bool prune_clients() {
  lock_guard<mutex> guard(clients_lock);
  bool pruned = false;
  for (auto it = clients.begin(), next = it; it != clients.end(); it = next) {
    next++;
    tm t = *localtime(&(it->second));
    t.tm_sec += CLIENT_EXPIRY;
    if (mktime(&t) < time(0)) {
      clients.erase(it);
      remove_alarms(it->first);
      pruned = true;
    }
  }
  return pruned;
}

void send_alarms() {
  string alarms_msg = get_alarms();
  {
    lock_guard<mutex> guard(clients_lock);
    for (auto it = clients.begin(); it != clients.end(); it++) {
      try {
        s.send(it->first, alarms_msg, websocketpp::frame::opcode::text);
      } catch (...) {
        // Ignore.
      }
    }
  }
}

void on_open(connection_hdl hdl) {
  lock_guard<mutex> guard(clients_lock);
  clients[hdl] = time(0);
}

void on_close(connection_hdl hdl) {
  {
    lock_guard<mutex> guard(clients_lock);
    clients.erase(hdl);
  }
  prune_clients();
  send_alarms();
}

void on_message(connection_hdl hdl, server::message_ptr msg) {
  {
    lock_guard<mutex> guard(alarms_lock);
    string payload = msg->get_payload();
    if (payload.rfind("start:", 0) == 0) {
      string client = payload.substr(6);
      time_t t = time(0);
      cout << "start: " << client << " (" << get_time_string(t) << ")" << endl;
      alarms[client] = make_pair(hdl, t);
    } else if (payload.rfind("stop:", 0) == 0) {
      string client = payload.substr(5);
      auto it = alarms.find(client);
      if (it != alarms.end()) {
        time_t t = it->second.second;
        cout << "stop:  " << client << " (" << get_time_string(t) << ")" << endl;
        alarms.erase(it);
      }
    }
  }
  prune_clients();
  send_alarms();
}

bool on_ping(connection_hdl hdl, string msg) {
  {
    lock_guard<mutex> guard(clients_lock);
    clients[hdl] = time(0);
  }

  if (prune_clients()) {
    send_alarms();
  }

  return true;
}

void signal_callback_handler(int signum) {
  s.stop_listening();
  s.stop();
}

int main() {
  s.set_error_channels(websocketpp::log::elevel::none);
  s.set_access_channels(websocketpp::log::alevel::none);
  s.init_asio();

  s.set_open_handler(on_open);
  s.set_close_handler(on_close);
  s.set_message_handler(on_message);
  s.set_ping_handler(on_ping);

  signal(SIGINT, signal_callback_handler);

  s.listen(PORT);
  s.start_accept();
  s.run();

  return 0;
}
