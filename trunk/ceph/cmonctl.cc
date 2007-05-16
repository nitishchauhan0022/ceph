// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */

#include <sys/stat.h>
#include <iostream>
#include <string>
using namespace std;

#include "config.h"

#include "mon/MonMap.h"
#include "msg/SimpleMessenger.h"
#include "messages/MMonCommand.h"
#include "messages/MMonCommandAck.h"

#include "common/Timer.h"

#ifndef DARWIN
#include <envz.h>
#endif // DARWIN

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


Messenger *messenger = 0;

class Admin : public Dispatcher {
  void dispatch(Message *m) {
    switch (m->get_type()) {
    case MSG_MON_COMMAND_ACK:
      dout(0) << m->get_source() << " -> '"
	      << ((MMonCommandAck*)m)->rs << "' (" << ((MMonCommandAck*)m)->r << ")"
	      << endl;
      messenger->shutdown();
      break;      
    }
  }
} dispatcher;

int main(int argc, char **argv, char *envp[]) {

  vector<char*> args;
  argv_to_vec(argc, argv, args);
  parse_config_options(args);

  // args for fuse
  vec_to_argv(args, argc, argv);

  // load monmap
  MonMap monmap;
  int r = monmap.read(".ceph_monmap");
  assert(r >= 0);
  
  // build command
  MMonCommand *m = new MMonCommand;
  string cmd;
  for (unsigned i=0; i<args.size(); i++) {
    if (i) cmd += " ";
    cmd += args[i];
    m->cmd.push_back(string(args[i]));
  }
  int mon = monmap.pick_mon();

  dout(0) << "mon" << mon << " <- '" << cmd << "'" << endl;

  // start up network
  rank.start_rank();
  messenger = rank.register_entity(entity_name_t(entity_name_t::TYPE_ADMIN));
  messenger->set_dispatcher(&dispatcher);
  
  // send it
  messenger->send_message(m, monmap.get_inst(mon));

  // wait for messenger to finish
  rank.wait();
  
  return 0;
}

