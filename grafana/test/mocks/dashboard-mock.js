/*! grafana - v1.9.1 - 2014-12-29
 * Copyright (c) 2014 Torkel Ödegaard; Licensed Apache License */

define([],function(){return{create:function(){return{title:"",tags:[],style:"dark",timezone:"browser",editable:!0,failover:!1,panel_hints:!0,rows:[],pulldowns:[{type:"templating"},{type:"annotations"}],nav:[{type:"timepicker"}],time:{from:"1h",to:"now"},templating:{list:[]},refresh:"10s"}}}});