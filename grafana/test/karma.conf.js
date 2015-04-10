/*! grafana - v1.9.1 - 2014-12-29
 * Copyright (c) 2014 Torkel Ödegaard; Licensed Apache License */

module.exports=function(a){a.set({basePath:"../../",frameworks:["mocha","requirejs","expect","sinon"],files:["src/test/test-main.js",{pattern:"src/app/**/*.js",included:!1},{pattern:"src/vendor/**/*.js",included:!1},{pattern:"src/test/**/*.js",included:!1},{pattern:"src/**/*.js",included:!1}],exclude:[],reporters:["dots"],port:9876,colors:!0,logLevel:a.LOG_INFO,autoWatch:!0,browsers:["PhantomJS"],captureTimeout:6e4,singleRun:!0})};