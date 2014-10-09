/*! grafana - v1.5.4 - 2014-05-13
 * Copyright (c) 2014 Torkel Ödegaard; Licensed Apache License */

module.exports=function(a){a.set({basePath:"../",frameworks:["mocha","requirejs","expect"],files:["test/test-main.js",{pattern:"app/**/*.js",included:!1},{pattern:"vendor/**/*.js",included:!1},{pattern:"test/**/*.js",included:!1},{pattern:"**/*.js",included:!1}],exclude:[],reporters:["progress"],port:9876,colors:!0,logLevel:a.LOG_INFO,autoWatch:!0,browsers:["Chrome"],captureTimeout:6e4,singleRun:!1})};