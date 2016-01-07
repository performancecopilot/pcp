/*! grafana - v1.9.1 - 2016-01-07
 * Copyright (c) 2016 Torkel Ödegaard; Licensed Apache License */

define(["angular","app","lodash","require"],function(a,b,c){"use strict";var d=a.module("grafana.panels.custom",[]);b.useModule(d),d.controller("CustomPanelCtrl",["$scope","panelSrv",function(a,b){a.panelMeta={description:"Example plugin panel"};var d={};c.defaults(a.panel,d),a.init=function(){b.init(a)},a.init()}])});