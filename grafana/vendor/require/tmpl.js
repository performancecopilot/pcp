/*! grafana - v1.9.1 - 2016-01-07
 * Copyright (c) 2016 Torkel Ödegaard; Licensed Apache License */

define(["module"],function(a){"use strict";var b=a.config&&a.config()||{};return{load:function(a,c,d,e){var f=c.toUrl(a);c(["text!"+a],function(a){b.registerTemplate&&b.registerTemplate(f,a),d(a)})}}});