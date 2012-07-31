import unittest
import pmapi
import time
import sys
import argparse
import copy
import pymongo
import uuid
from pcp import *
from ctypes import *
from pymongo import *

# _connect ------------------------------------------------------------------

class _connect():
    def __init__(self, mongo_port, mongo_address):
    
    ## connection ##
        try:
            self.connection = Connection()
            self.connection = Connection(mongo_address, mongo_port)
            self.db = self.connection.mongo_database
            #print 'connection: {}'.format(self.connection)
            self.collection = self.db.mongo_collection
            self._networkinfo = self.db._networkinfo
            self._meminfo = self.db._meminfo
            self._diskinfo = self.db._diskinfo
            self._procinfo = self.db._procinfo
            self._interruptinfo = self.db._interruptinfo
            self._cpuinfo = self.db._cpuinfo
            
        except:
            print "Unexpected Database Connection Error: ", sys.exc_info()[0]
            print "Ensure mongod service has been started and is running."
            raise


# main ----------------------------------------------------
if __name__ == '__main__':

    i = 1
    mongo_connect = False
    mongo_port = 27017
    mongo_address = 'localhost'
    subsys = set()
    disk = 'd'
    cpu = 'c'
    net = 'n'
    interrupt = 'j'
    memory = 'm'
    s_options = {"d":[disk,"brief"],"D":[disk,"detail"],
                 "c":[cpu,"brief"],"C":[cpu,"detail"],
                 "n":[net,"brief"],"N":[net,"detail"],
                 "j":[interrupt,"brief"],"J":[interrupt,"detail"],
                 "m":[memory,"brief"],"M":[memory,"detail"],
                 }

    while i < len(sys.argv):
        if (sys.argv[i][:2] == "-s"):
            for j in xrange(len(sys.argv[i][2:])):
                subsys_arg = sys.argv[i][j+2:j+3]
                subsys.add(s_options[subsys_arg][0])
        elif (sys.argv[i] == "--mongo-connect"):
            mongo_connect = True
        elif (sys.argv[i] == "--bind-ip"):
            i += 1
            mongo_address = sys.argv[i]
        elif (sys.argv[i] == "--port"):
            i += 1
            mongo_port = int(sys.argv[i])
        i += 1
    if(mongo_connect == True):
        dbconnect = _connect(mongo_port, mongo_address)

    try:
        for s in subsys:
            if s == 0: continue
            if(mongo_connect):
                if s == 'c':
                    for posts in dbconnect.db._cpuinfo.find():
                        print '{}\n'.format(posts)
                elif s == 'j':
                    for posts in dbconnect.db._interruptinfo.find():
                        print '{}\n'.format(posts)
                elif s == 'm':
                    for posts in dbconnect.db._memoryinfo.find():
                        print '{}\n'.format(posts)
                elif s == 'd':
                    for posts in dbconnect.db._diskinfo.find():
                        print '{}\n'.format(posts)
                elif s == 'n':
                    for posts in dbconnect.db._networkinfo.find():
                        print '{}\n'.format(posts)
    except KeyboardInterrupt:
        True
