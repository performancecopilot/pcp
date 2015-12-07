/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
!function(){"use strict";function e(e){e.errorClassnames.push("alert-danger")}e.$inject=["flashProvider"],angular.module("app.routes",["ngRoute"]),angular.module("vector.config",[]),angular.module("vector.version",[]),angular.module("app.charts",[]),angular.module("app.filters",[]),angular.module("app.metrics",[]),angular.module("app.datamodels",[]),angular.module("app.services",[]),angular.module("app",["app.routes","ui.dashboard","app.controllers","app.datamodels","app.widgets","app.charts","app.services","app.filters","app.metrics","vector.config","vector.version","angular-flash.service","angular-flash.flash-alert-directive"]).config(e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(){function e(){return Math.floor(65536*(1+Math.random())).toString(16).substring(1)}return{getGuid:e}}angular.module("app.services").factory("VectorService",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i,a){function r(t){var r=i.properties.protocol+"://"+i.properties.host+":"+i.properties.port,n={};return n.method="GET",n.url=r+"/pmapi/context",n.params={},n.params[t.contextType]=t.contextValue,n.params.polltimeout=t.pollTimeout.toString(),n.timeout=5e3,e(n).then(function(e){return e.data.context?e.data.context:a.reject("context is undefined")})}function n(e,t){var i={};return i.contextType="hostspec",i.contextValue=e,i.pollTimeout=t,r(i)}function o(e,t){var i={};return i.contextType="hostname",i.contextValue=e,i.pollTimeout=t,r(i)}function s(e){var t={};return t.contextType="local",t.contextValue="ANYTHING",t.pollTimeout=e,r(t)}function c(e,t){var i={};return i.contextType="archivefile",i.contextValue=e,i.pollTimeout=t,r(i)}function l(t,r,n){var o=i.properties.protocol+"://"+i.properties.host+":"+i.properties.port,s={};return s.method="GET",s.url=o+"/pmapi/"+t+"/_fetch",s.params={},angular.isDefined(r)&&null!==r&&(s.params.names=r.join(",")),angular.isDefined(n)&&null!==n&&(s.params.pmids=n.join(",")),e(s).then(function(e){return angular.isUndefined(e.data.timestamp)||angular.isUndefined(e.data.timestamp.s)||angular.isUndefined(e.data.timestamp.us)||angular.isUndefined(e.data.values)?a.reject("metric values is empty"):e})}function d(t,r,n,o){var s=i.properties.protocol+"://"+i.properties.host+":"+i.properties.port,c={};return c.method="GET",c.url=s+"/pmapi/"+t+"/_indom",c.params={indom:r},angular.isDefined(n)&&null!==n&&(c.params.instance=n.join(",")),angular.isDefined(o)&&null!==o&&(c.params.inames=o.join(",")),c.cache=!0,e(c).then(function(e){return angular.isDefined(e.data.indom)||angular.isDefined(e.data.instances)?e:a.reject("instances is undefined")})}function u(t,r,n,o){var s=i.properties.protocol+"://"+i.properties.host+":"+i.properties.port,c={};return c.method="GET",c.url=s+"/pmapi/"+t+"/_indom",c.params={name:r},angular.isDefined(n)&&null!==n&&(c.params.instance=n.join(",")),angular.isDefined(o)&&null!==o&&(c.params.inames=o.join(",")),c.cache=!0,e(c).then(function(e){return angular.isDefined(e.data.instances)?e:a.reject("instances is undefined")})}function p(e){var t=1e3*e.data.timestamp.s+e.data.timestamp.us/1e3,i=e.data.values;return{timestamp:t,values:i}}function m(e){var t={};return angular.forEach(e,function(e){var i=e.data.indom,a=e.config.params.name,r={};angular.forEach(e.data.instances,function(e){r[e.instance.toString()]=e.name}),t[a.toString()]={indom:i,name:a,inames:r}}),t}function g(e,t){var i=a.defer(),r=[];return angular.forEach(t.values,function(t){var i=_.map(t.instances,function(e){return angular.isDefined(e.instance)&&null!==e.instance?e.instance:-1});r.push(u(e,t.name,i))}),a.all(r).then(function(e){var a=m(e),r={timestamp:t.timestamp,values:t.values,inames:a};i.resolve(r)},function(e){i.reject(e)},function(e){i.update(e)}),i.promise}function h(e,t){return l(e,t).then(p).then(function(t){return g(e,t)})}return{getHostspecContext:n,getHostnameContext:o,getLocalContext:s,getArchiveContext:c,getMetricsValues:l,getMetrics:h,getInstanceDomainsByIndom:d,getInstanceDomainsByName:u}}angular.module("app.services").factory("PMAPIService",e),e.$inject=["$http","$log","$rootScope","$q"]}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i,a,r,n,o,s,c,l,d){function u(e){var t=_.find(w,function(t){return t.name===e});return angular.isUndefined(t)?(t=new n(e),w.push(t)):t.subscribers++,t}function p(e){var t=_.find(w,function(t){return t.name===e});return angular.isUndefined(t)?(t=new o(e),w.push(t)):t.subscribers++,t}function m(e,t){var i=_.find(w,function(t){return t.name===e});return angular.isUndefined(i)?(i=new s(e,t),w.push(i)):i.subscribers++,i}function g(e,t){var i=_.find(w,function(t){return t.name===e});return angular.isUndefined(i)?(i=new c(e,t),w.push(i)):i.subscribers++,i}function h(e,t){var i=_.find(C,function(t){return t.name===e});return angular.isUndefined(i)?(i=new l(e,t),C.push(i)):i.subscribers++,i}function f(e){var t,i=_.find(w,function(t){return t.name===e});i.subscribers--,i.subscribers<1&&(t=w.indexOf(i),t>-1&&w.splice(t,1))}function v(e){var t,i=_.find(C,function(t){return t.name===e});i.subscribers--,i.subscribers<1&&(t=C.indexOf(i),t>-1&&C.splice(t,1))}function y(){angular.forEach(w,function(e){e.clearData()})}function b(){angular.forEach(C,function(e){e.clearData()})}function M(t){var i,a=[],n=e.properties.host,o=e.properties.port,s=e.properties.context,c=e.properties.protocol;s&&s>0&&w.length>0&&(angular.forEach(w,function(e){a.push(e.name)}),i=c+"://"+n+":"+o+"/pmapi/"+s+"/_fetch?names="+a.join(","),r.getMetrics(s,a).then(function(e){angular.forEach(e.values,function(t){var i=t.name;angular.forEach(t.instances,function(t){var a=angular.isUndefined(t.instance)?1:t.instance,r=e.inames[i].inames[a],n=_.find(w,function(e){return e.name===i});angular.isDefined(n)&&null!==n&&n.pushValue(e.timestamp,a,r,t.value)})})}).then(function(){t(!0)},function(){d.to("alert-dashboard-error").error="Failed fetching metrics.",t(!1)}),e.$broadcast("updateMetrics"))}function k(){C.length>0&&(angular.forEach(C,function(e){e.updateValues()}),e.$broadcast("updateDerivedMetrics"))}var w=[],C=[];return{getOrCreateMetric:u,getOrCreateCumulativeMetric:p,getOrCreateConvertedMetric:m,getOrCreateCumulativeConvertedMetric:g,getOrCreateDerivedMetric:h,destroyMetric:f,destroyDerivedMetric:v,clearMetricList:y,clearDerivedMetricList:b,updateMetrics:M,updateDerivedMetrics:k}}e.$inject=["$rootScope","$http","$log","$q","PMAPIService","SimpleMetric","CumulativeMetric","ConvertedMetric","CumulativeConvertedMetric","DerivedMetric","flash"],angular.module("app.services").factory("MetricListService",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){return{getInames:function(e,a){return i.getInstanceDomainsByName(t.properties.context,e,[a])}}}e.$inject=["$http","$rootScope","PMAPIService"],angular.module("app.services").factory("MetricService",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i,a){function r(){i.get(t.properties.protocol+"://"+t.properties.host+":"+t.properties.port+"/pmapi/"+t.properties.context+"/_fetch?names=generic.heatmap").success(function(){a.to("alert-disklatency-success").success="generic.heatmap requested!",e.info("generic.heatmap requested")}).error(function(){a.to("alert-disklatency-error").error="failed requesting generic.heatmap!",e.error("failed requesting generic.heatmap")})}return{generate:r}}e.$inject=["$log","$rootScope","$http","flash"],angular.module("app.services").factory("HeatMapService",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i,a){function r(){i.get(t.properties.protocol+"://"+t.properties.host+":"+t.properties.port+"/pmapi/"+t.properties.context+"/_fetch?names=generic.systack").success(function(){a.to("alert-sysstack").success="generic.systack requested!",e.info("generic.systack requested")}).error(function(){a.to("alert-sysstack").error="failed requesting generic.systack!",e.error("failed requesting generic.systack")})}return{generate:r}}e.$inject=["$log","$rootScope","$http","flash"],angular.module("app.services").factory("FlameGraphService",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i,a,r,n,o,s,c,l){function d(){C&&(i.cancel(C),a.info("Interval canceled."))}function u(e){e?x=0:x+=1,x>5&&(d(C),x=0,s.to("alert-dashboard-error").error="Consistently failed fetching metrics from host (>5). Aborting loop. Please make sure PCP is running correctly.")}function p(){o.updateMetrics(u),o.updateDerivedMetrics()}function m(){d(C),e.properties.host&&(e.properties.context&&e.properties.context>0?C=i(p,1e3*e.properties.interval):s.to("alert-dashboard-error").error="Invalid context. Please update host to resume operation.",a.info("Interval updated."))}function g(t){e.properties.hostname=t.values[0].instances[0].value,a.info("Hostname updated: "+e.properties.hostname)}function h(){e.properties.hostname="Hostname not available.",a.error("Error fetching hostname.")}function f(t){e.flags.contextAvailable=!0,e.properties.context=t,m()}function v(){e.flags.contextAvailable=!1,a.error("Error fetching context.")}function y(t){a.info("Context updated.");var i=e.properties.hostspec,r=null;t&&""!==t&&(e.flags.contextUpdating=!0,e.flags.contextAvailable=!1,r=t.match("(.*):([0-9]*)"),null!==r?(e.properties.host=r[1],e.properties.port=r[2]):e.properties.host=t,n.getHostspecContext(i,600).then(function(t){e.flags.contextUpdating=!1,f(t),n.getMetrics(t,["pmcd.hostname"]).then(function(e){g(e)},function(){h()})},function(){s.to("alert-dashboard-error").error="Failed fetching context from host. Try updating the hostname.",e.flags.contextUpdating=!1,v()}))}function b(t){a.info("Host updated."),r.search("host",t),r.search("hostspec",e.properties.hostspec),e.properties.context=-1,e.properties.hostname=null,e.properties.port=c.port,o.clearMetricList(),o.clearDerivedMetricList(),y(t)}function M(){a.log("Window updated.")}function k(e){a.log("Global Filter updated."),l.setGlobalFilter(e)}function w(){e.properties?(e.properties.interval||(e.properties.interval=c.interval),e.properties.window||(e.properties.window=c.window),e.properties.protocol||(e.properties.protocol=c.protocol),e.properties.host||(e.properties.host=""),e.properties.hostspec||(e.properties.hostspec=c.hostspec),e.properties.port||(e.properties.port=c.port),!e.properties.context||e.properties.context<0?y():m()):e.properties={protocol:c.protocol,host:"",hostspec:c.hostspec,port:c.port,context:-1,hostname:null,window:c.window,interval:c.interval},e.flags={contextAvailable:!1,contextUpdating:!1},c.enableContainerWidgets&&l.initContainerCgroups()}var C,x=0;return{updateContext:y,cancelInterval:d,updateInterval:m,updateHost:b,updateWindow:M,updateGlobalFilter:k,initializeProperties:w}}e.$inject=["$rootScope","$http","$interval","$log","$location","PMAPIService","MetricListService","flash","vectorConfig","ContainerMetadataService"],angular.module("app.services").factory("DashboardService",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(){function e(){return function(e){return d3.time.format("%X")(new Date(e))}}function t(){return function(e){return d3.format(".02f")(e)}}function i(){return function(e){return d3.format("f")(e)}}function a(){return function(e){return d3.format("%")(e)}}function r(){return function(e){return e.x}}function n(){return function(e){return e.y}}function o(){return"chart_"+Math.floor(65536*(1+Math.random())).toString(16).substring(1)}return{xAxisTickFormat:e,yAxisTickFormat:t,yAxisIntegerTickFormat:i,yAxisPercentageTickFormat:a,xFunction:r,yFunction:n,getId:o}}angular.module("app.services").factory("D3Service",e)}(),function(){"use strict";function e(e,t,i,a,r,n){function o(e){return y[s(e)]}function s(e){return-1!==e.indexOf("docker/")?e=e.split("/")[2]:-1!==e.indexOf("/docker-")&&(e=e.split("-")[1].split(".")[0]),e}function c(){y={}}function l(e){r.externalAPI||o(e,e)}function d(e){return void 0!==y[s(e)]}function u(){v=n.getOrCreateMetric("containers.cgroup"),a(p,1e3*t.properties.interval)}function p(){y=v.data.reduce(function(e,t){return f(t.values[t.values.length-1].x)?e[t.key]=t.key.substring(0,12):delete e[t.key],e},{})}function m(e){b=e}function g(e){return""===b||-1!==e.indexOf(b)}function h(e){e>M&&(M=e)}function f(e){var t=M-e;return 6e3>t}var v,y={},b="",M=0;return{idDictionary:o,clearIdDictionary:c,resolveId:l,setGlobalFilter:m,checkGlobalFilter:g,setCurrentTime:h,isTimeCurrent:f,containerIdExist:d,initContainerCgroups:u}}e.$inject=["$http","$rootScope","$q","$interval","containerConfig","MetricListService"],angular.module("app.services").factory("ContainerMetadataService",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e){var t=function(e){this.name=e||null,this.data=[],this.subscribers=1};return t.prototype.toString=function(){return this.name},t.prototype.pushValue=function(t,i,a,r){var n,o,s=this;n=_.find(s.data,function(e){return e.iid===i}),angular.isDefined(n)&&null!==n?(n.values.push({x:t,y:r}),o=n.values.length-60*e.properties.window/e.properties.interval,o>0&&n.values.splice(0,o)):(n={key:angular.isDefined(a)?a:this.name,iid:i,values:[{x:t,y:r},{x:t+1,y:r}]},s.data.push(n))},t.prototype.clearData=function(){this.data.length=0},t}e.$inject=["$rootScope"],angular.module("app.metrics").factory("SimpleMetric",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e){var t=function(e,t){this.name=e,this.data=[],this.subscribers=1,this.derivedFunction=t};return t.prototype.updateValues=function(){var t,i=this;t=i.derivedFunction(),t.length!==i.data.length&&(i.data.length=0),angular.forEach(t,function(t){var a,r=_.find(i.data,function(e){return e.key===t.key});angular.isUndefined(r)?(r={key:t.key,values:[{x:t.timestamp,y:t.value},{x:t.timestamp+1,y:t.value}]},i.data.push(r)):(r.values.push({x:t.timestamp,y:t.value}),a=r.values.length-60*e.properties.window/e.properties.interval,a>0&&r.values.splice(0,a))})},t.prototype.clearData=function(){this.data.length=0},t}e.$inject=["$rootScope"],angular.module("app.metrics").factory("DerivedMetric",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(e,t){this.base=a,this.base(e),this.conversionFunction=t};return a.prototype=new i,a.prototype.pushValue=function(t,i,a,r){var n,o,s,c,l=this;n=_.find(l.data,function(e){return e.iid===i}),angular.isUndefined(n)?(n={key:angular.isDefined(a)?a:this.name,iid:i,values:[],previousValue:r,previousTimestamp:t},l.data.push(n)):(s=(r-n.previousValue)/(t-n.previousTimestamp),c=l.conversionFunction(s),n.values.push({x:t,y:c}),n.previousValue=r,n.previousTimestamp=t,o=n.values.length-60*e.properties.window/e.properties.interval,o>0&&n.values.splice(0,o))},a}e.$inject=["$rootScope","$log","SimpleMetric"],angular.module("app.metrics").factory("CumulativeConvertedMetric",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(e){this.base=i,this.base(e)};return a.prototype=new i,a.prototype.pushValue=function(t,i,a,r){var n,o,s,c=this;n=_.find(c.data,function(e){return e.iid===i}),angular.isUndefined(n)?(n={key:angular.isDefined(a)?a:this.name,iid:i,values:[],previousValue:r,previousTimestamp:t},c.data.push(n)):(s=(r-n.previousValue)/((t-n.previousTimestamp)/1e3),n.values.length<1?n.values.push({x:t,y:s},{x:t+1,y:s}):n.values.push({x:t,y:s}),n.previousValue=r,n.previousTimestamp=t,o=n.values.length-60*e.properties.window/e.properties.interval,o>0&&n.values.splice(0,o))},a}e.$inject=["$rootScope","$log","SimpleMetric"],angular.module("app.metrics").factory("CumulativeMetric",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(e,t){this.base=i,this.base(e),this.conversionFunction=t};return a.prototype=new i,a.prototype.pushValue=function(t,i,a,r){var n,o,s,c=this;s=c.conversionFunction(r),n=_.find(c.data,function(e){return e.iid===i}),angular.isDefined(n)&&null!==n?(n.values.push({x:t,y:s}),o=n.values.length-60*e.properties.window/e.properties.interval,o>0&&n.values.splice(0,o)):(n={key:angular.isDefined(a)?a:this.name,iid:i,values:[{x:t,y:s},{x:t+1,y:s}]},c.data.push(n))},a}e.$inject=["$rootScope","$log","SimpleMetric"],angular.module("app.metrics").factory("ConvertedMetric",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metric=t.getOrCreateMetric(this.name),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyMetric(this.name),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("MetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,r=t.getOrCreateCumulativeMetric("kernel.percpu.cpu.sys"),n=t.getOrCreateCumulativeMetric("kernel.percpu.cpu.user");a=function(){var e,t,i,a=[];return angular.forEach(r.data,function(r){r.values.length>0&&(e=_.find(n.data,function(e){return e.key===r.key}),angular.isDefined(e)&&(t=r.values[r.values.length-1],i=e.values[e.values.length-1],t.x===i.x&&a.push({timestamp:t.x,key:r.key,value:(t.y+i.y)/1e3})))}),a},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("kernel.percpu.cpu.sys"),t.destroyMetric("kernel.percpu.cpu.user"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("PerCpuUtilizationMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,r=t.getOrCreateCumulativeMetric("network.interface.in.bytes"),n=t.getOrCreateCumulativeMetric("network.interface.out.bytes");a=function(){var e,t=[],i=function(i,a){i.values.length>0&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:i.key+a,value:e.y/1024}))};return angular.forEach(r.data,function(e){i(e," in")}),angular.forEach(n.data,function(e){i(e," out")}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("network.interface.in.bytes"),t.destroyMetric("network.interface.out.bytes"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("NetworkBytesMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metricDefinitions=this.dataModelOptions.metricDefinitions;var a,r={};angular.forEach(this.metricDefinitions,function(e,i){r[i]=t.getOrCreateMetric(e)}),a=function(){var e,t=[];return angular.forEach(r,function(i,a){angular.forEach(i.data,function(i){i.values.length>0&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:a.replace("{key}",i.key),value:e.y}))})}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),angular.forEach(this.metricDefinitions,function(e){t.destroyMetric(e)}),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("MultipleMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metricDefinitions=this.dataModelOptions.metricDefinitions;var a,r={};angular.forEach(this.metricDefinitions,function(e,i){r[i]=t.getOrCreateCumulativeMetric(e)}),a=function(){var e,t=[];return angular.forEach(r,function(i,a){angular.forEach(i.data,function(i){i.values.length>0&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:a.replace("{key}",i.key),value:e.y}))})}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),angular.forEach(this.metricDefinitions,function(e){t.destroyMetric(e)}),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("MultipleCumulativeMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,r=function(e){return e/1024},n=t.getOrCreateConvertedMetric("mem.util.cached",r),o=t.getOrCreateConvertedMetric("mem.util.used",r),s=t.getOrCreateConvertedMetric("mem.util.free",r),c=t.getOrCreateConvertedMetric("mem.util.bufmem",r);a=function(){var e,t,i,a,r=[];return e=function(){if(o.data.length>0){var e=o.data[o.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),t=function(){if(n.data.length>0){var e=n.data[n.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),i=function(){if(s.data.length>0){var e=s.data[s.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),a=function(){if(c.data.length>0){var e=c.data[c.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),angular.isDefined(e)&&angular.isDefined(t)&&angular.isDefined(a)&&r.push({timestamp:e.x,key:"application",value:e.y-t.y-a.y}),angular.isDefined(t)&&angular.isDefined(a)&&r.push({timestamp:e.x,key:"free (cache)",value:t.y+a.y}),angular.isDefined(i)&&r.push({timestamp:e.x,key:"free (unused)",value:i.y}),r},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("mem.util.cached"),t.destroyMetric("mem.util.used"),t.destroyMetric("mem.util.free"),t.destroyMetric("mem.util.bufmem"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("MemoryUtilizationMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t){var i=function(){return this};return i.prototype=Object.create(e.prototype),i.prototype.init=function(){e.prototype.init.call(this),this.metric=t.getOrCreateMetric("kernel.uname.release")},i.prototype.destroy=function(){t.destroyMetric("kernel.uname.release"),e.prototype.destroy.call(this)},i}e.$inject=["WidgetDataModel","MetricListService"],angular.module("app.datamodels").factory("DummyMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,r=t.getOrCreateCumulativeMetric("disk.dev.read_rawactive"),n=t.getOrCreateCumulativeMetric("disk.dev.write_rawactive"),o=t.getOrCreateCumulativeMetric("disk.dev.read"),s=t.getOrCreateCumulativeMetric("disk.dev.write");a=function(){function e(e,r,n,o){e.data.length>0&&angular.forEach(e.data,function(e){t=_.find(r.data,function(t){return t.key===e.key}),angular.isDefined(t)&&e.values.length>0&&t.values.length>0&&(i=e.values[e.values.length-1],a=t.values[e.values.length-1],c=i.y>0?a.y/i.y:0,o.push({timestamp:i.x,key:e.key+n,value:c}))})}var t,i,a,c,l=[];return e(o,r," read latency",l),e(s,n," write latency",l),l},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("disk.dev.read_rawactive"),t.destroyMetric("disk.dev.write_rawactive"),t.destroyMetric("disk.dev.read"),t.destroyMetric("disk.dev.write"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("DiskLatencyMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,r=t.getOrCreateCumulativeMetric(this.name);a=function(){var e,t=[];return angular.forEach(r.data,function(i){i.values.length>0&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:i.key,value:e.y/1e3}))}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric(this.name),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("CumulativeUtilizationMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metric=t.getOrCreateCumulativeMetric(this.name),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyMetric(this.name),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("CumulativeMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,r=t.getOrCreateCumulativeMetric("kernel.all.cpu.sys"),n=t.getOrCreateCumulativeMetric("kernel.all.cpu.user"),o=t.getOrCreateMetric("hinv.ncpu");a=function(){var e,t,i=[];if(o.data.length>0&&(e=o.data[o.data.length-1],e.values.length>0)){t=e.values[e.values.length-1].y;var a=function(e,a){if(e.values.length>0){var r=e.values[e.values.length-1];i.push({timestamp:r.x,key:a,value:r.y/(1e3*t)})}};angular.forEach(r.data,function(e){a(e,"sys")}),angular.forEach(n.data,function(e){a(e,"user")})}return i},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("kernel.all.cpu.sys"),t.destroyMetric("kernel.all.cpu.user"),t.destroyMetric("hinv.ncpu"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("CpuUtilizationMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i,a){var r=function(){return this};return r.prototype=Object.create(e.prototype),r.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var r,n=t.getOrCreateCumulativeMetric("network.interface.in.bytes"),o=t.getOrCreateCumulativeMetric("network.interface.out.bytes");r=function(){var e,t=[];return angular.forEach(n.data,function(i){a.setCurrentTime(i.previousTimestamp),i.values.length>0&&-1!==i.key.indexOf("veth")&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:i.key+" in",value:e.y/1024}))}),angular.forEach(o.data,function(i){i.values.length>0&&-1!==i.key.indexOf("veth")&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:i.key+" out",value:e.y/1024}))}),t},this.metric=t.getOrCreateDerivedMetric("container.network.interface",r),this.updateScope(this.metric.data)},r.prototype.destroy=function(){t.destroyDerivedMetric("container.network.interface"),t.destroyMetric("network.interface.in.bytes"),t.destroyMetric("network.interface.out.bytes"),e.prototype.destroy.call(this)},r}e.$inject=["WidgetDataModel","MetricListService","VectorService","ContainerMetadataService"],angular.module("app.datamodels").factory("ContainerNetworkBytesMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metricDefinitions=this.dataModelOptions.metricDefinitions;var a,r={};angular.forEach(this.metricDefinitions,function(e,i){r[i]=t.getOrCreateCumulativeMetric(e)}),a=function(){var e,t=[];return angular.forEach(r,function(i,a){angular.forEach(i.data,function(i){i.values.length>0&&-1!==i.key.indexOf("docker/")&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:a.replace("{key}",i.key),value:e.y}))})}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),angular.forEach(this.metricDefinitions,function(e){t.destroyMetric(e)}),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("ContainerMultipleCumulativeMetricDataModel",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,r=function(e){return e/1024},n=t.getOrCreateConvertedMetric("mem.util.cached",r),o=t.getOrCreateConvertedMetric("mem.util.used",r),s=t.getOrCreateConvertedMetric("mem.freemem",r),c=t.getOrCreateConvertedMetric("mem.util.bufmem",r),l=t.getOrCreateCumulativeMetric("cgroup.memory.usage");a=function(){var e,t,i,a,r,d,u=[];return e=function(){if(o.data.length>0){var e=o.data[o.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),t=function(){if(n.data.length>0){var e=n.data[n.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),i=function(){if(s.data.length>0){var e=s.data[s.data.length-1];if(e.values.length>0)return d=e.values[e.values.length-1].x,e.values[e.values.length-1]}}(),a=function(){if(c.data.length>0){var e=c.data[c.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),r=function(){var e=0;return angular.forEach(l.data,function(t){var i=d-t.previousTimestamp;t.values.length>0&&-1!==t.key.indexOf("docker/")&&5500>i&&(e+=t.previousValue/1024/1024)}),Math.round(e)}(),angular.isDefined(e)&&angular.isDefined(r)&&u.push({timestamp:e.x,key:"System used mem",value:e.y-r}),angular.isDefined(i)&&u.push({timestamp:e.x,key:"System free (unused)",value:i.y}),angular.isDefined(r)&&angular.isDefined(e)&&u.push({timestamp:e.x,key:"Container used mem",value:r}),u},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("mem.util.used"),t.destroyMetric("mem.freemem"),t.destroyMetric("cgroup.memory.usage"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("ContainerMemoryUtilizationMetricDataModel",e)}(),function(){"use strict";function e(e,t,i,a,r){var n=function(){return this};return n.prototype=Object.create(i.prototype),n.prototype.init=function(){i.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+r.getGuid();var t,n=a.getOrCreateCumulativeMetric("cgroup.memory.usage");t=function(){var t,i=[];return angular.forEach(n.data,function(a){if(a.values.length>0&&e.containerIdExist(a.key)){e.resolveId(a.key),t=a.values[a.values.length-1];var r=e.idDictionary(a.key)||a.key;e.checkGlobalFilter(r)&&i.push({timestamp:t.x,key:r,value:a.previousValue/1024/1024})}}),i},this.metric=a.getOrCreateDerivedMetric(this.name,t),this.updateScope(this.metric.data)},n.prototype.destroy=function(){a.destroyDerivedMetric(this.name),a.destroyMetric("cgroup.memory.usage"),i.prototype.destroy.call(this)},n}e.$inject=["ContainerMetadataService","$rootScope","WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("ContainerMemoryBytesMetricTimeSeriesDataModel",e)}(),function(){"use strict";function e(e,t,i,a){var r=function(){return this};return r.prototype=Object.create(t.prototype),r.prototype.init=function(){t.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+a.getGuid();var r,n=i.getOrCreateCumulativeMetric("cgroup.cpuacct.stat.user"),o=i.getOrCreateCumulativeMetric("cgroup.cpuacct.stat.system");r=function(){var t=[],i=[];return n.data.length>0&&o.data.length>0&&(angular.forEach(n.data,function(t){e.setCurrentTime(t.previousTimestamp),t.values.length>0&&e.containerIdExist(t.key)&&(i.push(t.previousValue),e.resolveId(t.key))}),angular.forEach(o.data,function(a){if(a.values.length>0&&e.containerIdExist(a.key)){var r=a.values[a.values.length-1],n=e.idDictionary(a.key)||a.key;e.checkGlobalFilter(n)&&t.push({timestamp:r.x,key:n,value:a.previousValue/i.shift()/100})}})),t},this.metric=i.getOrCreateDerivedMetric(this.name,r),this.updateScope(this.metric.data)},r.prototype.destroy=function(){i.destroyDerivedMetric(this.name),i.destroyMetric("cgroup.cpuacct.stat.user"),i.destroyMetric("cgroup.cpuacct.stat.system"),t.prototype.destroy.call(this)},r}e.$inject=["ContainerMetadataService","WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("ContainerCPUstatMetricTimeSeriesDataModel",e)}(),nv.models.tooltip=function(){function e(){if(!u){var e=document.body;u=d3.select(e).append("div").attr("class","nvtooltip "+(s?s:"xy-tooltip")).attr("id",i),u.style("top",0).style("left",0),u.style("opacity",0),u.style("position","fixed"),u.selectAll("div, table, td, tr").classed(f,!0),u.classed(f,!0)}}function t(){return m&&w(a)?(nv.dom.write(function(){e();var t=k(a);t&&(u.node().innerHTML=t),x()}),t):void 0}var i="nvtooltip-"+Math.floor(1e5*Math.random()),a=null,r="w",n=25,o=0,s=null,c=null,l=!0,d=200,u=null,p={left:null,top:null},m=!0,g=100,h=!0,f="nv-pointer-events-none",v=function(){return{left:d3.event.clientX,top:d3.event.clientY}},y=function(e){return e},b=function(e){return e},M=function(e){return e},k=function(e){if(null===e)return"";var t=d3.select(document.createElement("table"));if(h){var i=t.selectAll("thead").data([e]).enter().append("thead");i.append("tr").append("td").attr("colspan",3).append("strong").classed("x-value",!0).html(b(e.value))}var a=t.selectAll("tbody").data([e]).enter().append("tbody"),r=a.selectAll("tr").data(function(e){return e.series}).enter().append("tr").classed("highlight",function(e){return e.highlight});r.append("td").classed("legend-color-guide",!0).append("div").style("background-color",function(e){return e.color}),r.append("td").classed("key",!0).classed("total",function(e){return!!e.total}).html(function(e,t){return M(e.key,t)}),r.append("td").classed("value",!0).html(function(e,t){return y(e.value,t)}),r.selectAll("td").each(function(e){if(e.highlight){var t=d3.scale.linear().domain([0,1]).range(["#fff",e.color]),i=.6;d3.select(this).style("border-bottom-color",t(i)).style("border-top-color",t(i))}});var n=t.node().outerHTML;return void 0!==e.footer&&(n+='<div class="footer">'+e.footer+"</div>"),n},w=function(e){if(e&&e.series){if(e.series instanceof Array)return!!e.series.length;if(e.series instanceof Object)return e.series=[e.series],!0}return!1},C=function(e){var t,i,a,o=u.node().offsetHeight,s=u.node().offsetWidth,c=document.documentElement.clientWidth,l=document.documentElement.clientHeight;switch(r){case"e":t=-s-n,i=-(o/2),e.left+t<0&&(t=n),(a=e.top+i)<0&&(i-=a),(a=e.top+i+o)>l&&(i-=a-l);break;case"w":t=n,i=-(o/2),e.left+t+s>c&&(t=-s-n),(a=e.top+i)<0&&(i-=a),(a=e.top+i+o)>l&&(i-=a-l);break;case"n":t=-(s/2)-5,i=n,e.top+i+o>l&&(i=-o-n),(a=e.left+t)<0&&(t-=a),(a=e.left+t+s)>c&&(t-=a-c);break;case"s":t=-(s/2),i=-o-n,e.top+i<0&&(i=n),(a=e.left+t)<0&&(t-=a),(a=e.left+t+s)>c&&(t-=a-c);break;case"center":t=-(s/2),i=-(o/2);break;default:t=0,i=0}return{left:t,top:i}},x=function(){nv.dom.read(function(){var e=v(),t=C(e),i=e.left+t.left,a=e.top+t.top;if(l)u.interrupt().transition().delay(d).duration(0).style("opacity",0);else{var r="translate("+p.left+"px, "+p.top+"px)",n="translate("+i+"px, "+a+"px)",o=d3.interpolateString(r,n),s=u.style("opacity")<.1;u.interrupt().transition().duration(s?0:g).styleTween("transform",function(){return o},"important").styleTween("-webkit-transform",function(){return o}).style("-ms-transform",n).style("opacity",1)}p.left=i,p.top=a})};return t.nvPointerEventsClass=f,t.options=nv.utils.optionsFunc.bind(t),t._options=Object.create({},{duration:{get:function(){return g},set:function(e){g=e}},gravity:{get:function(){return r},set:function(e){r=e}},distance:{get:function(){return n},set:function(e){n=e}},snapDistance:{get:function(){return o},set:function(e){o=e}},classes:{get:function(){return s},set:function(e){s=e}},chartContainer:{get:function(){return c},set:function(e){c=e}},enabled:{get:function(){return m},set:function(e){m=e}},hideDelay:{get:function(){return d},set:function(e){d=e}},contentGenerator:{get:function(){return k},set:function(e){k=e}},valueFormatter:{get:function(){return y},set:function(e){y=e}},headerFormatter:{get:function(){return b},set:function(e){b=e}},keyFormatter:{get:function(){return M},set:function(e){M=e}},headerEnabled:{get:function(){return h},set:function(e){h=e}},position:{get:function(){return v},set:function(e){v=e}},hidden:{get:function(){return l},set:function(e){l!==e&&(l=!!e,t())}},data:{get:function(){return a},set:function(e){e.point&&(e.value=e.point.x,e.series=e.series||{},e.series.value=e.point.y,e.series.color=e.point.color||e.series.color),a=e}},node:{get:function(){return u.node()},set:function(){}},id:{get:function(){return i},set:function(){}}}),nv.utils.initOptions(t),t},/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){function a(t){t.id=i.getId(),t.flags=e.flags;var a;nv.addGraph(function(){var e=250;return a=nv.models.lineChart().options({duration:0,useInteractiveGuideline:!0,interactive:!1,showLegend:!0,showXAxis:!0,showYAxis:!0}),a.margin({left:35,right:35}),a.height(e),t.forcey&&a.forceY([0,t.forcey]),a.x(i.xFunction()),a.y(i.yFunction()),a.xAxis.tickFormat(i.xAxisTickFormat()),a.yAxis.tickFormat(t.percentage?i.yAxisPercentageTickFormat():t.integer?i.yAxisIntegerTickFormat():i.yAxisTickFormat()),nv.utils.windowResize(a.update),d3.select("#"+t.id+" svg").datum(t.data).style("height",e+"px").transition().duration(0).call(a),a}),t.$on("updateMetrics",function(){a.update()})}return{restrict:"A",templateUrl:"app/charts/nvd3-chart.html",scope:{data:"=",percentage:"=",integer:"=",forcey:"="},link:a}}e.$inject=["$rootScope","$log","D3Service"],angular.module("app.charts").directive("lineTimeSeries",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i,a){function r(r){r.host=e.properties.host,r.port=e.properties.port,r.context=e.properties.context,r.ready=!1,r.processing=!1,r.id=a.getGuid(),r.generateHeatMap=function(){i.generate(),r.ready=!0,r.processing=!0,t(function(){r.processing=!1},15e4)}}return{restrict:"A",templateUrl:"app/charts/disk-latency-heatmap.html",link:r}}e.$inject=["$rootScope","$timeout","HeatMapService","VectorService"],angular.module("app.charts").directive("diskLatencyHeatMap",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i,a){function r(r){r.host=e.properties.host,r.port=e.properties.port,r.context=e.properties.context,r.ready=!1,r.processing=!1,r.id=a.getGuid(),r.generateFlameGraph=function(){i.generate(),r.ready=!0,r.processing=!0,t(function(){r.processing=!1},65e3)}}return{restrict:"A",templateUrl:"app/charts/cpu-flame-graph.html",link:r}}e.$inject=["$rootScope","$timeout","FlameGraphService","VectorService"],angular.module("app.charts").directive("cpuFlameGraph",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i){function a(t){t.id=i.getId(),t.flags=e.flags,t.legend=!0;var a;nv.addGraph(function(){var e=i.yAxisTickFormat(),r=250;return a=nv.models.stackedAreaChart().options({duration:0,useInteractiveGuideline:!0,interactive:!1,showLegend:!0,showXAxis:!0,showYAxis:!0,showControls:!1}),a.margin({left:35,right:35}),a.height(r),t.forcey&&a.yDomain([0,t.forcey]),a.x(i.xFunction()),a.y(i.yFunction()),a.xAxis.tickFormat(i.xAxisTickFormat()),t.percentage?(e=i.yAxisPercentageTickFormat(),a.yAxis.tickFormat()):t.integer&&(e=i.yAxisIntegerTickFormat(),a.yAxis.tickFormat()),a.yAxis.tickFormat(e),a.interactiveLayer.tooltip.contentGenerator(function(t){var i=t.value,a='<thead><tr><td colspan="3"><strong class="x-value">'+i+"</strong></td></tr></thead>",r="<tbody>",n=t.series;return n.forEach(function(t){r=r+'<tr><td class="legend-color-guide"><div style="background-color: '+t.color+';"></div></td><td class="key">'+t.key+'</td><td class="value">'+e(t.value)+"</td></tr>"}),r+="</tbody>","<table>"+a+r+"</table>"}),nv.utils.windowResize(a.update),d3.select("#"+t.id+" svg").datum(t.data).style("height",r+"px").transition().duration(0).call(a),a}),t.$on("updateMetrics",function(){a.update()})}return{restrict:"A",templateUrl:"app/charts/nvd3-chart.html",scope:{data:"=",percentage:"=",integer:"=",forcey:"="},link:a}}e.$inject=["$rootScope","$log","D3Service"],angular.module("app.charts").directive("areaStackedTimeSeries",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i,a,r,n,o,s,c,l,d){function u(){e[0].hidden||e[0].webkitHidden||e[0].mozHidden||e[0].msHidden?l.cancelInterval():l.updateInterval()}function p(){l.initializeProperties(),r.protocol&&(t.properties.protocol=r.protocol,i.info("Protocol: "+r.protocol)),r.host&&(m.inputHost=r.host,i.info("Host: "+r.host),r.hostspec&&(t.properties.hostspec=r.hostspec,i.info("Hostspec: "+r.hostspec)),l.updateHost(m.inputHost)),e[0].addEventListener("visibilitychange",u,!1),e[0].addEventListener("webkitvisibilitychange",u,!1),e[0].addEventListener("msvisibilitychange",u,!1),e[0].addEventListener("mozvisibilitychange",u,!1),i.info("Dashboard controller initialized with "+g+" view.")}var m=this,g=a.current.$$route.originalPath,h=s;if(void 0!==r.widgets){var f=r.widgets.split(",")||[];h=f.reduce(function(e,t){return e.concat(o.filter(function(e){return e.name===t}))},[])}else{var v=s.reduce(function(e,t){return e.push(t.name),e},[]).join();n.search("widgets",v)}m.dashboardOptions={hideToolbar:!0,widgetButtons:!1,hideWidgetName:!0,hideWidgetSettings:!0,widgetDefinitions:o,defaultWidgets:h},m.version=d.id,m.embed=c,m.addWidgetToURL=function(e){var t="";void 0===r.widgets?r.widgets="":t=",",e.length?(r.widgets="",t=e.reduce(function(e,t){return e.push(t.name),e},[]).join()):t+=e.name,n.search("widgets",r.widgets+t)},m.removeWidgetFromURL=function(e){for(var t=r.widgets.split(",")||[],i=0;i<t.length;i++)if(t[i]===e.name){t.splice(i,1);break}t.length<1?n.search("widgets",null):n.search("widgets",t.toString())},m.removeAllWidgetFromURL=function(){n.search("widgets",null)},m.updateGlobalFilter=function(){l.updateGlobalFilter(m.globalFilter)},m.updateInterval=l.updateInterval,m.updateHost=function(){l.updateHost(m.inputHost)},m.updateWindow=l.updateWindow,m.globalFilter="",m.isHostnameExpanded=!1,m.inputHost="",p()}e.$inject=["$document","$rootScope","$log","$route","$routeParams","$location","widgetDefinitions","widgets","embed","DashboardService","vectorVersion"],angular.module("app.controllers",[]).controller("DashboardController",e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t,i,a,r,n,o,s,c,l,d,u,p,m,g,h,f){var v=[{name:"kernel.all.load",title:"Load Average",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"kernel.all.load"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"},{name:"kernel.all.runnable",title:"Runnable",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"kernel.all.runnable"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:4,integer:!0}},{name:"kernel.all.cpu.sys",title:"CPU Utilization (System)",directive:"line-time-series",dataAttrName:"data",dataModelType:h,dataModelOptions:{name:"kernel.all.cpu.sys"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.all.cpu.user",title:"CPU Utilization (User)",directive:"line-time-series",dataAttrName:"data",dataModelType:h,dataModelOptions:{name:"kernel.all.cpu.user"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.all.cpu",title:"CPU Utilization",directive:"area-stacked-time-series",dataAttrName:"data",dataModelType:l,dataModelOptions:{name:"kernel.all.cpu"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.percpu.cpu.sys",title:"Per-CPU Utilization (System)",directive:"line-time-series",dataAttrName:"data",dataModelType:h,dataModelOptions:{name:"kernel.percpu.cpu.sys"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.percpu.cpu.user",title:"Per-CPU Utilization (User)",directive:"line-time-series",dataAttrName:"data",dataModelType:h,dataModelOptions:{name:"kernel.percpu.cpu.user"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.percpu.cpu",title:"Per-CPU Utilization",directive:"line-time-series",dataAttrName:"data",dataModelType:d,dataModelOptions:{name:"kernel.percpu.cpu"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"mem.freemem",title:"Memory Utilization (Free)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"mem.freemem"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Memory"},{name:"mem.util.used",title:"Memory Utilization (Used)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"mem.util.used"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Memory"},{name:"mem.util.cached",title:"Memory Utilization (Cached)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"mem.util.cached"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Memory"},{name:"mem",title:"Memory Utilization",directive:"area-stacked-time-series",dataAttrName:"data",dataModelType:s,dataModelOptions:{name:"mem"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Memory",attrs:{percentage:!1,integer:!0}},{name:"network.interface.out.drops",title:"Network Drops (Out)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.interface.out.drops"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{forcey:10,percentage:!1,integer:!0}},{name:"network.interface.in.drops",title:"Network Drops (In)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.interface.in.drops"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{forcey:10,percentage:!1,integer:!0}},{name:"network.interface.drops",title:"Network Drops",directive:"line-time-series",dataAttrName:"data",dataModelType:u,dataModelOptions:{name:"network.interface.drops",metricDefinitions:{"{key} in":"network.interface.in.drops","{key} out":"network.interface.out.drops"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{forcey:10,percentage:!1,integer:!0}},{name:"network.tcpconn.established",title:"TCP Connections (Estabilished)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.tcpconn.established"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{percentage:!1,integer:!0}},{name:"network.tcpconn.time_wait",title:"TCP Connections (Time Wait)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.tcpconn.time_wait"},enableVerticalResize:!1,size:{width:"25%",height:"250px"},group:"Network",attrs:{percentage:!1,integer:!0}},{name:"network.tcpconn.close_wait",title:"TCP Connections (Close Wait)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.tcpconn.close_wait"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{percentage:!1,integer:!0}},{name:"network.tcpconn",title:"TCP Connections",directive:"line-time-series",dataAttrName:"data",dataModelType:u,dataModelOptions:{name:"network.tcpconn",metricDefinitions:{established:"network.tcpconn.established",time_wait:"network.tcpconn.time_wait",close_wait:"network.tcpconn.close_wait"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{percentage:!1,integer:!0}},{name:"network.interface.bytes",title:"Network Throughput (kB)",directive:"line-time-series",dataAttrName:"data",dataModelType:c,dataModelOptions:{name:"network.interface.bytes"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{percentage:!1,integer:!0}},{name:"disk.iops",title:"Disk IOPS",directive:"line-time-series",dataAttrName:"data",dataModelType:p,dataModelOptions:{name:"disk.iops",metricDefinitions:{"{key} read":"disk.dev.read","{key} write":"disk.dev.write"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Disk"},{name:"disk.bytes",title:"Disk Throughput (Bytes)",directive:"line-time-series",dataAttrName:"data",dataModelType:p,dataModelOptions:{name:"disk.bytes",metricDefinitions:{"{key} read":"disk.dev.read_bytes","{key} write":"disk.dev.write_bytes"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Disk"},{name:"disk.dev.avactive",title:"Disk Utilization",directive:"line-time-series",dataAttrName:"data",dataModelType:h,dataModelOptions:{name:"disk.dev.avactive"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Disk",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.all.pswitch",title:"Context Switches",directive:"line-time-series",dataAttrName:"data",dataModelType:t,dataModelOptions:{name:"kernel.all.pswitch"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{percentage:!1,integer:!0}},{name:"mem.vmstat.pgfault",title:"Page Faults",directive:"area-stacked-time-series",dataAttrName:"data",dataModelType:p,dataModelOptions:{name:"mem.vmstat.pgfault",metricDefinitions:{"page faults":"mem.vmstat.pgfault","major page faults":"mem.vmstat.pgmajfault"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Memory",attrs:{percentage:!1,integer:!0}},{name:"network.interface.packets",title:"Network Packets",directive:"line-time-series",dataAttrName:"data",dataModelType:p,dataModelOptions:{name:"network.interface.packets",metricDefinitions:{"{key} in":"network.interface.in.packets","{key} out":"network.interface.out.packets"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{percentage:!1,integer:!0}},{name:"network.tcp.retrans",title:"Network Retransmits",directive:"line-time-series",dataAttrName:"data",dataModelType:p,dataModelOptions:{name:"network.tcp.retrans",metricDefinitions:{retranssegs:"network.tcp.retranssegs",timeouts:"network.tcp.timeouts",listendrops:"network.tcp.listendrops",fastretrans:"network.tcp.fastretrans",slowstartretrans:"network.tcp.slowstartretrans",syncretrans:"network.tcp.syncretrans"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{forcey:10,percentage:!1,integer:!0}},{name:"disk.dev.latency",title:"Disk Latency",directive:"line-time-series",dataAttrName:"data",dataModelType:g,dataModelOptions:{name:"disk.dev.latency"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Disk",attrs:{percentage:!1,integer:!0}}];return f.enableCpuFlameGraph&&v.push({name:"graph.flame.cpu",title:"CPU Flame Graph",directive:"cpu-flame-graph",dataModelType:m,size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"}),f.enableDiskLatencyHeatMap&&v.push({name:"graph.heatmap.disk",title:"Disk Latency Heat Map",directive:"disk-latency-heat-map",dataModelType:m,size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Disk"}),f.enableContainerWidgets&&v.push({name:"cgroup.cpuacct.stat.user",title:"Container CPU Utilization",directive:"area-stacked-time-series",dataAttrName:"data",dataModelType:i,dataModelOptions:{name:"cgroup.cpuacct.stat.user"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Container",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"cgroup.memory.usage",title:"Container Memory Usage",directive:"line-time-series",dataAttrName:"data",dataModelType:a,dataModelOptions:{name:"cgroup.memory.usage"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Container"},{name:"memory.Headroom",title:"Container Memory Headroom",directive:"area-stacked-time-series",dataAttrName:"data",dataModelType:r,dataModelOptions:{name:"mem"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Container",attrs:{percentage:!1,integer:!0}},{name:"container.network.interface.bytes",title:"Container Network Throughput (kB)",directive:"line-time-series",dataAttrName:"data",dataModelType:n,dataModelOptions:{name:"network.interface.bytes"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Container",attrs:{percentage:!1,integer:!0}},{name:"container.disk.iops",title:"Container Disk IOPS",directive:"line-time-series",dataAttrName:"data",dataModelType:o,dataModelOptions:{name:"container.disk.iops",metricDefinitions:{"{key} read":"cgroup.blkio.all.io_serviced.read","{key} write":"cgroup.blkio.all.io_serviced.write"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Container"},{name:"container.disk.bytes",title:"Container Disk Throughput (kB)",directive:"line-time-series",dataAttrName:"data",dataModelType:o,dataModelOptions:{name:"container.disk.bytes",metricDefinitions:{"{key} read":"cgroup.blkio.all.io_service_bytes.read","{key} write":"cgroup.blkio.all.io_service_bytes.write"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Container"}),v}e.$inject=["MetricDataModel","CumulativeMetricDataModel","ContainerCPUstatMetricTimeSeriesDataModel","ContainerMemoryBytesMetricTimeSeriesDataModel","ContainerMemoryUtilizationMetricDataModel","ContainerNetworkBytesMetricDataModel","ContainerMultipleCumulativeMetricDataModel","MemoryUtilizationMetricDataModel","NetworkBytesMetricDataModel","CpuUtilizationMetricDataModel","PerCpuUtilizationMetricDataModel","MultipleMetricDataModel","MultipleCumulativeMetricDataModel","DummyMetricDataModel","DiskLatencyMetricDataModel","CumulativeUtilizationMetricDataModel","vectorConfig"];var t=[{name:"kernel.all.cpu",size:{width:"25%"}},{name:"kernel.percpu.cpu",size:{width:"25%"}},{name:"kernel.all.runnable",size:{width:"25%"}},{name:"kernel.all.load",size:{width:"25%"}},{name:"network.interface.bytes",size:{width:"25%"}},{name:"network.tcpconn",size:{width:"25%"}},{name:"network.interface.packets",size:{width:"25%"}},{name:"network.tcp.retrans",size:{width:"25%"}},{name:"mem",size:{width:"50%"}},{name:"mem.vmstat.pgfault",size:{width:"25%"}},{name:"kernel.all.pswitch",size:{width:"25%"}},{name:"disk.iops",size:{width:"25%"}},{name:"disk.bytes",size:{width:"25%"}},{name:"disk.dev.avactive",size:{width:"25%"}},{name:"disk.dev.latency",size:{width:"25%"}}],i=[];angular.module("app.widgets",[]).factory("widgetDefinitions",e).value("defaultWidgets",t).value("emptyWidgets",i)}(),function(){"use strict";angular.module("vector.version").constant("vectorVersion",{id:"v1.0.3"})}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e){e.when("/",{templateUrl:"app/dashboard/dashboard.html",controller:"DashboardController",controllerAs:"vm",title:"Dashboard - Vector",reloadOnSearch:!1,resolve:{widgets:["defaultWidgets",function(e){return e}],embed:function(){return!1}}}).when("/embed",{templateUrl:"app/dashboard/dashboard.html",controller:"DashboardController",controllerAs:"vm",title:"Dashboard - Vector",reloadOnSearch:!1,resolve:{widgets:["defaultWidgets",function(e){return e}],embed:function(){return!0}}}).when("/empty",{templateUrl:"app/dashboard/dashboard.html",controller:"DashboardController",controllerAs:"vm",title:"Dashboard - Vector",reloadOnSearch:!1,resolve:{widgets:["emptyWidgets",function(e){return e}],embed:function(){return!1}}}).otherwise("/")}e.$inject=["$routeProvider"],angular.module("app.routes").config(e)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";function e(e,t){var i,a,r=[];for(i=0;i<e.length;i++)a=e[i][t],-1===r.indexOf(a)&&r.push(a);return r}function t(){return function(t,i){return null!==t?e(t,i):void 0}}function i(){return function(e,t){return e.filter(function(e){return e.group===t})}}angular.module("app.filters").filter("groupBy",t).filter("groupFilter",i)}(),/**!
 *
 *  Copyright 2015 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */
function(){"use strict";angular.module("vector.config").constant("vectorConfig",{protocol:"http",port:44323,hostspec:"localhost",interval:2,window:2,enableCpuFlameGraph:!1,enableDiskLatencyHeatMap:!1,enableContainerWidgets:!1}).constant("containerConfig",{externalAPI:!1})}(),angular.module("app").run(["$templateCache",function(e){e.put("app/charts/cpu-flame-graph.html",'<div class="col-md-12"><div class="row" style="text-align: center;"><div id="alert-sysstack" flash-alert="" active-class="in alert" class="fade"><span class="alert-message">{{flash.message}}</span></div><p>Click on the button below to generate a CPU flame graph! (60 sec)</p><button type="button" class="btn btn-primary" ng-disabled="processing" ng-click="generateFlameGraph()">Generate Flame Graph <i ng-if="processing" class="fa fa-refresh fa-spin"></i></button></div><div class="row" ng-if="!processing && ready" style="text-align: center; margin-top: 15px;"><p>The CPU flame graph is ready. Please click on the button below to open it.</p><a class="btn btn-default" href="http://{{host}}:{{port}}/systack/systack.svg" target="_blank">Open Flame Graph</a></div></div>'),e.put("app/charts/disk-latency-heatmap.html",'<div class="col-md-12"><div class="row" style="text-align: center;"><p class="lead" ng-if="!ready">Click on the button below to generate your Disk Latency Heat Map! (140 sec)</p></div><div class="row" ng-if="!processing && ready" style="text-align: center;"><object data="http://{{host}}:{{port}}/heatmap/heatmap.svg" type="image/svg+xml" style="width:100%; height:auto;"></object><br><a class="btn btn-default pull-right" href="http://{{host}}:{{port}}/heatmap/heatmap.svg" target="_blank">Open in a new Tab</a></div><div class="row"><div id="alert-disklatency-error" flash-alert="error" active-class="in alert" class="fade"><strong class="alert-heading">Error!</strong> <span class="alert-message">{{flash.message}}</span></div></div><div class="row"><div id="alert-disklatency-success" flash-alert="success" active-class="in alert" class="fade"><strong class="alert-heading">Success!</strong> <span class="alert-message">{{flash.message}}</span></div></div><div class="row" style="text-align: center; margin-top: 15px;"><button type="button" class="btn btn-primary btn-lg" ng-disabled="processing" ng-click="generateHeatMap()">Generate <i ng-if="processing" class="fa fa-refresh fa-spin"></i></button></div></div>'),e.put("app/charts/nvd3-chart.html",'<div><i class="fa fa-line-chart fa-5x widget-icon" ng-hide="flags.contextAvailable"></i><div id="{{id}}" class="chart" ng-hide="!flags.contextAvailable"><svg></svg></div></div>'),e.put("app/dashboard/dashboard.html",'<div class="navbar navbar-inverse navbar-fixed-top" role="navigation" ng-if="!vm.embed"><div class="container-fluid"><div class="navbar-header"><button type="button" class="navbar-toggle" data-toggle="collapse" data-target=".navbar-collapse"><span class="sr-only">Toggle navigation</span> <span class="icon-bar"></span> <span class="icon-bar"></span> <span class="icon-bar"></span></button> <a class="navbar-brand" href="#/"><img ng-src="assets/images/vector_owl.png" alt="Vector Owl" height="20"></a> <a class="navbar-brand" href="#/">Vector</a></div></div></div><div class="dashboard-container" ng-class="{ \'main-container\': !vm.embed }"><div class="row"><div class="col-md-12"><div dashboard="vm.dashboardOptions" template-url="app/dashboard/vector.dashboard.html" class="dashboard-container"></div></div></div></div>'),e.put("app/dashboard/vector.dashboard.html",'<div class="row" ng-if="!vm.embed"><div class="col-md-6"><form role="form" name="form"><div class="input-group input-group-lg" ng-class="{\'has-error\': form.host.$invalid && form.host.$dirty}"><span class="input-group-addon" ng-click="vm.isHostnameExpanded = !vm.isHostnameExpanded">Hostname &nbsp; <i class="fa fa-plus-square-o"></i></span> <input type="text" class="form-control" id="hostnameInput" name="host" data-content="Please enter the instance hostname. Port can be specified using the <hostname>:<port> format. Expand to change hostspec." rel="popover" data-placement="bottom" data-trigger="hover" data-container="body" ng-model="vm.inputHost" ng-change="vm.updateHost()" ng-model-options="{ updateOn: \'default\', debounce: 1000 }" delay="1000" ng-disabled="flags.contextUpdating == true" required="" placeholder="Instance Hostname"> <i class="fa fa-refresh fa-2x form-control-feedback" ng-if="flags.contextUpdating"></i> <i class="fa fa-check fa-2x form-control-feedback" ng-if="flags.contextAvailable"></i></div></form></div><div class="btn-group col-md-2" id="widgetWrapper"><button id="widgetButton" type="button" class="dropdown-toggle btn btn-lg btn-default btn-block" data-toggle="dropdown">Widget <span class="caret"></span></button><ul class="dropdown-menu" role="menu"><li class="dropdown-submenu" ng-repeat="group in widgetDefs | groupBy: \'group\'"><a ng-click="void(0)" data-toggle="dropdown">{{group}}</a><ul class="dropdown-menu"><li ng-repeat="widget in widgetDefs | groupFilter: group"><a href="#" ng-click="addWidgetInternal($event, widget); vm.addWidgetToURL(widget);">{{widget.title}}</a></li></ul></li><li role="presentation" class="divider"></li><li><a href="javascript:void(0);" ng-click="loadWidgets(defaultWidgets); vm.addWidgetToURL(defaultWidgets);">Default Widgets</a></li><li><a href="javascript:void(0);" ng-click="loadWidgets(emptyWidgets); vm.removeAllWidgetFromURL();">Clear</a></li></ul></div><div class="col-md-2"><form role="form"><div class="input-group input-group-lg" ng-class="{\'has-error\': form.window.$invalid && form.window.$dirty}" style="border-radius: 6 !important;"><span class="input-group-addon">Window</span><select class="form-control" name="window" id="windowInput" data-content="The duration window for all charts in this dashboard." rel="popover" data-placement="bottom" data-trigger="hover" data-container="body" ng-model="properties.window" ng-change="vm.updateWindow()" style="border-radius: 6 !important;"><option value="2">2 min</option><option value="5">5 min</option><option value="10">10 min</option></select></div></form></div><div class="col-md-2"><form role="form"><div class="input-group input-group-lg" ng-class="{\'has-error\': form.interval.$invalid && form.interval.$dirty}"><span class="input-group-addon">Interval</span><select class="form-control" name="interval" id="intervalInput" data-content="The update interval used by all charts in this dashboard." rel="popover" data-placement="bottom" data-trigger="hover" data-container="body" ng-model="properties.interval" ng-change="vm.updateInterval()"><option value="1">1 sec</option><option value="2">2 sec</option><option value="3">3 sec</option><option value="5">5 sec</option></select></div></form></div></div><div class="row" ng-show="vm.isHostnameExpanded" ng-if="!vm.embed"><div class="col-md-6"><form role="form" name="form"><div class="input-group input-group-lg" ng-class="{\'has-error\': form.hostspec.$invalid && form.hostspec.$dirty}"><span class="input-group-addon">Hostspec&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span> <input type="text" class="form-control" id="hostspecInput" name="host" data-content="Please enter the instance hostspec." rel="popover" data-placement="bottom" data-trigger="hover" data-container="body" ng-model="properties.hostspec" ng-change="vm.updateHost()" ng-model-options="{ updateOn: \'default\', debounce: 1000 }" delay="1000" ng-disabled="flags.contextUpdating == true" required="" placeholder="Instance Hostspec"></div><div class="input-group input-group-lg"><span class="input-group-addon">Filter&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span> <input type="text" class="form-control" id="globalFilterInput" name="globalFilter" data-content="Global filter for container widgets" rel="popover" data-placement="bottom" data-trigger="hover" data-container="body" ng-model="vm.globalFilter" ng-change="vm.updateGlobalFilter()" ng-model-options="{ updateOn: \'default\', debounce: 1000 }" delay="1000" placeholder="Container Filter"></div></form></div><div class="col-md-2"><p class="lead" id="hostnameLabel" data-content="PMCD hostname. The hostname from the actual instance you\'re monitoring." rel="popover" data-placement="bottom" data-trigger="hover" data-container="body">{{flags.contextAvailable && properties.hostname ? properties.hostname : \'Disconnected\'}}</p></div></div><div id="alert-dashboard-error" flash-alert="error" active-class="in alert" class="fade"><strong class="alert-heading">Error!</strong> <span class="alert-message">{{flash.message}}</span></div><div class="row"><div class="col-md-12"><div class="btn-toolbar" ng-if="!options.hideToolbar"><div class="btn-group" ng-if="!options.widgetButtons"><button type="button" class="dropdown-toggle btn btn-primary" data-toggle="dropdown">Add Widget <span class="caret"></span></button><ul class="dropdown-menu" role="menu"><li ng-repeat="widget in widgetDefs"><a href="#" ng-click="addWidgetInternal($event, widget);"><span class="label label-primary">{{widget.name}}</span></a></li></ul></div><div class="btn-group" ng-if="options.widgetButtons"><button ng-repeat="widget in widgetDefs" ng-click="addWidgetInternal($event, widget);" type="button" class="btn btn-primary">{{widget.name}}</button></div><button class="btn btn-warning" ng-click="resetWidgetsToDefault()">Default Widgets</button> <button ng-if="options.storage && options.explicitSave" ng-click="options.saveDashboard()" class="btn btn-success" ng-disabled="!options.unsavedChangeCount">{{ !options.unsavedChangeCount ? "all saved" : "save changes (" + options.unsavedChangeCount + ")" }}</button> <button ng-click="clear();" type="button" class="btn btn-info">Clear</button></div><div ui-sortable="sortableOptions" ng-model="widgets" class="dashboard-widget-area"><div ng-repeat="widget in widgets" ng-style="widget.containerStyle" class="widget-container" widget=""><div class="widget panel panel-default"><div class="widget-header panel-heading"><h3 class="panel-title"><span class="widget-title">{{widget.title}}</span><span class="label label-primary" ng-if="!options.hideWidgetName">{{widget.name}}</span> <span ng-click="removeWidget(widget); vm.removeWidgetFromURL(widget);" class="glyphicon glyphicon-remove" ng-if="!options.hideWidgetClose"></span> <span ng-click="openWidgetDialog(widget);" class="glyphicon glyphicon-cog" ng-if="!options.hideWidgetSettings"></span></h3></div><div class="panel-body widget-content" ng-style="widget.contentStyle"></div><div class="widget-ew-resizer" ng-mousedown="grabResizer($event)"></div><div ng-if="widget.enableVerticalResize" class="widget-s-resizer" ng-mousedown="grabSouthResizer($event)"></div></div></div></div></div></div><div class="row" style="padding-left: 15px;" ng-if="!vm.embed">Version: {{vm.version}}</div><script>\n    (function () {\n        \'use strict\';\n        $(\'#hostnameInput\').popover();\n        $(\'#hostspecInput\').popover();\n        $(\'#globalFilterInput\').popover();\n        $(\'#windowInput\').popover();\n        $(\'#intervalInput\').popover();\n        $(\'#hostnameLabel\').popover();\n    }());\n</script>')}]);