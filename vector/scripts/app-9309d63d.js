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
!function(){"use strict";function e(e){e.errorClassnames.push("alert-danger")}e.$inject=["flashProvider"],angular.module("app.routes",["ngRoute"]),angular.module("vector.config",[]),angular.module("app.directives",[]),angular.module("app.filters",[]),angular.module("app.metrics",[]),angular.module("app.datamodels",[]),angular.module("app.services",[]),angular.module("app",["app.routes","ui.dashboard","app.controllers","app.datamodels","app.widgets","app.directives","app.services","app.filters","app.metrics","vector.config","nvd3ChartDirectives","angular-flash.service","angular-flash.flash-alert-directive"]).config(e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){function n(t){var n="http://"+i.properties.host+":"+i.properties.port,r={};return r.method="GET",r.url=n+"/pmapi/context",r.params={},r.params[t.contextType]=t.contextValue,r.params.polltimeout=t.pollTimeout.toString(),r.timeout=5e3,e(r).then(function(e){return e.data.context?e.data.context:a.reject("context is undefined")})}function r(e,t){var i={};return i.contextType="hostspec",i.contextValue=e,i.pollTimeout=t,n(i)}function s(e,t){var i={};return i.contextType="hostname",i.contextValue=e,i.pollTimeout=t,n(i)}function o(e){var t={};return t.contextType="local",t.contextValue="ANYTHING",t.pollTimeout=e,n(t)}function c(e,t){var i={};return i.contextType="archivefile",i.contextValue=e,i.pollTimeout=t,n(i)}function l(t,n,r){var s="http://"+i.properties.host+":"+i.properties.port,o={};return o.method="GET",o.url=s+"/pmapi/"+t+"/_fetch",o.params={},angular.isDefined(n)&&null!==n&&(o.params.names=n.join(",")),angular.isDefined(r)&&null!==r&&(o.params.pmids=r.join(",")),e(o).then(function(e){return angular.isUndefined(e.data.timestamp)||angular.isUndefined(e.data.timestamp.s)||angular.isUndefined(e.data.timestamp.us)||angular.isUndefined(e.data.values)?a.reject("metric values is empty"):e})}function d(t,n,r,s){var o="http://"+i.properties.host+":"+i.properties.port,c={};return c.method="GET",c.url=o+"/pmapi/"+t+"/_indom",c.params={indom:n},angular.isDefined(r)&&null!==r&&(c.params.instance=r.join(",")),angular.isDefined(s)&&null!==s&&(c.params.inames=s.join(",")),c.cache=!0,e(c).then(function(e){return angular.isDefined(e.data.indom)||angular.isDefined(e.data.instances)?e:a.reject("instances is undefined")})}function u(t,n,r,s){var o="http://"+i.properties.host+":"+i.properties.port,c={};return c.method="GET",c.url=o+"/pmapi/"+t+"/_indom",c.params={name:n},angular.isDefined(r)&&null!==r&&(c.params.instance=r.join(",")),angular.isDefined(s)&&null!==s&&(c.params.inames=s.join(",")),c.cache=!0,e(c).then(function(e){return angular.isDefined(e.data.instances)?e:a.reject("instances is undefined")})}function p(e){var t=1e3*e.data.timestamp.s+e.data.timestamp.us/1e3,i=e.data.values;return{timestamp:t,values:i}}function h(e){var t={};return angular.forEach(e,function(e){var i=e.data.indom,a=e.config.params.name,n={};angular.forEach(e.data.instances,function(e){n[e.instance.toString()]=e.name}),t[a.toString()]={indom:i,name:a,inames:n}}),t}function m(e,t){var i=a.defer(),n=[];return angular.forEach(t.values,function(t){var i=_.map(t.instances,function(e){return angular.isDefined(e.instance)&&null!==e.instance?e.instance:-1});n.push(u(e,t.name,i))}),a.all(n).then(function(e){var a=h(e),n={timestamp:t.timestamp,values:t.values,inames:a};i.resolve(n)},function(e){i.reject(e)},function(e){i.update(e)}),i.promise}function g(e,t){return l(e,t).then(p).then(function(t){return m(e,t)})}return{getHostspecContext:r,getHostnameContext:s,getLocalContext:o,getArchiveContext:c,getMetricsValues:l,getMetrics:g,getInstanceDomainsByIndom:d,getInstanceDomainsByName:u}}angular.module("app.services").factory("PMAPIService",e),e.$inject=["$http","$log","$rootScope","$q"]}(),/**!
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
function(){"use strict";function e(e,t,i,a,n,r,s,o,c,l,d){function u(e){var t=_.find(b,function(t){return t.name===e});return angular.isUndefined(t)?(t=new r(e),b.push(t)):t.subscribers++,t}function p(e){var t=_.find(b,function(t){return t.name===e});return angular.isUndefined(t)?(t=new s(e),b.push(t)):t.subscribers++,t}function h(e,t){var i=_.find(b,function(t){return t.name===e});return angular.isUndefined(i)?(i=new o(e,t),b.push(i)):i.subscribers++,i}function m(e,t){var i=_.find(b,function(t){return t.name===e});return angular.isUndefined(i)?(i=new c(e,t),b.push(i)):i.subscribers++,i}function g(e,t){var i=_.find(M,function(t){return t.name===e});return angular.isUndefined(i)?(i=new l(e,t),M.push(i)):i.subscribers++,i}function f(e){var t,i=_.find(b,function(t){return t.name===e});i.subscribers--,i.subscribers<1&&(t=b.indexOf(i),t>-1&&b.splice(t,1))}function v(e){var t,i=_.find(M,function(t){return t.name===e});i.subscribers--,i.subscribers<1&&(t=M.indexOf(i),t>-1&&M.splice(t,1))}function y(){angular.forEach(b,function(e){e.clearData()})}function w(){angular.forEach(M,function(e){e.clearData()})}function k(t){var i,a=[],r=e.properties.host,s=e.properties.port,o=e.properties.context;o&&o>0&&b.length>0&&(angular.forEach(b,function(e){a.push(e.name)}),i="http://"+r+":"+s+"/pmapi/"+o+"/_fetch?names="+a.join(","),n.getMetrics(o,a).then(function(e){angular.forEach(e.values,function(t){var i=t.name;angular.forEach(t.instances,function(t){var a=angular.isUndefined(t.instance)?1:t.instance,n=e.inames[i].inames[a],r=_.find(b,function(e){return e.name===i});angular.isDefined(r)&&null!==r&&r.pushValue(e.timestamp,a,n,t.value)})})}).then(function(){t(!0)},function(){d.to("alert-dashboard-error").error="Failed fetching metrics.",t(!1)}))}function x(){M.length>0&&angular.forEach(M,function(e){e.updateValues()})}var b=[],M=[];return{getOrCreateMetric:u,getOrCreateCumulativeMetric:p,getOrCreateConvertedMetric:h,getOrCreateCumulativeConvertedMetric:m,getOrCreateDerivedMetric:g,destroyMetric:f,destroyDerivedMetric:v,clearMetricList:y,clearDerivedMetricList:w,updateMetrics:k,updateDerivedMetrics:x}}e.$inject=["$rootScope","$http","$log","$q","PMAPIService","SimpleMetric","CumulativeMetric","ConvertedMetric","CumulativeConvertedMetric","DerivedMetric","flash"],angular.module("app.services").factory("MetricListService",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){function n(){i.get("http://"+t.properties.host+":"+t.properties.port+"/pmapi/"+t.properties.context+"/_fetch?names=generic.heatmap").success(function(){a.to("alert-disklatency-success").success="generic.heatmap requested!",e.info("generic.heatmap requested")}).error(function(){a.to("alert-disklatency-error").error="failed requesting generic.heatmap!",e.error("failed requesting generic.heatmap")})}return{generate:n}}e.$inject=["$log","$rootScope","$http","flash"],angular.module("app.services").factory("HeatMapService",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){function n(){i.get("http://"+t.properties.host+":"+t.properties.port+"/pmapi/"+t.properties.context+"/_fetch?names=generic.systack").success(function(){a.to("alert-sysstack-success").success="generic.systack requested!",e.info("generic.systack requested")}).error(function(){a.to("alert-sysstack-error").error="failed requesting generic.systack!",e.error("failed requesting generic.systack")})}return{generate:n}}e.$inject=["$log","$rootScope","$http","flash"],angular.module("app.services").factory("FlameGraphService",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a,n,r,s,o,c){function l(){x&&(i.cancel(x),a.info("Interval canceled."))}function d(e){e?b=0:b+=1,b>5&&(l(x),b=0,o.to("alert-dashboard-error").error="Consistently failed fetching metrics from host (>5). Aborting loop. Please make sure PCP is running correctly.")}function u(){s.updateMetrics(d),s.updateDerivedMetrics()}function p(){l(x),e.properties.host&&(e.properties.context&&e.properties.context>0?x=i(u,1e3*e.properties.interval):o.to("alert-dashboard-error").error="Invalid context. Please update host to resume operation.",a.info("Interval updated."))}function h(t){e.properties.hostname=t.values[0].instances[0].value,a.info("Hostname updated: "+e.properties.hostname)}function m(){e.properties.hostname="Hostname not available.",a.error("Error fetching hostname.")}function g(t){e.flags.contextAvailable=!0,e.properties.context=t,p()}function f(){e.flags.contextAvailable=!1,a.error("Error fetching context.")}function v(t){a.info("Context updated.");var i=e.properties.hostspec,n=null;t&&""!==t&&(e.flags.contextUpdating=!0,e.flags.contextAvailable=!1,n=t.match("(.*):([0-9]*)"),null!==n?(e.properties.host=n[1],e.properties.port=n[2]):e.properties.host=t,r.getHostspecContext(i,600).then(function(t){e.flags.contextUpdating=!1,g(t),r.getMetrics(t,["pmcd.hostname"]).then(function(e){h(e)},function(){m()})},function(){o.to("alert-dashboard-error").error="Failed fetching context from host. Try updating the hostname.",e.flags.contextUpdating=!1,f()}))}function y(t){a.info("Host updated."),n.search("host",t),n.search("hostspec",e.properties.hostspec),e.properties.context=-1,e.properties.hostname=null,e.properties.port=c.port,s.clearMetricList(),s.clearDerivedMetricList(),v(t)}function w(){a.log("Window updated.")}function k(){e.properties?(e.properties.interval||(e.properties.interval=c.interval),e.properties.window||(e.properties.window=c.window),e.properties.host||(e.properties.host=""),e.properties.hostspec||(e.properties.hostspec=c.hostspec),e.properties.port||(e.properties.port=c.port),!e.properties.context||e.properties.context<0?v():p()):e.properties={host:"",hostspec:c.hostspec,port:c.port,context:-1,hostname:null,window:c.window,interval:c.interval},e.flags={contextAvailable:!1,contextUpdating:!1}}var x,b=0;return{updateContext:v,cancelInterval:l,updateInterval:p,updateHost:y,updateWindow:w,initializeProperties:k}}e.$inject=["$rootScope","$http","$interval","$log","$location","PMAPIService","MetricListService","flash","vectorConfig"],angular.module("app.services").factory("DashboardService",e)}(),/**!
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
function(){"use strict";function e(){function e(){return function(e){return d3.time.format("%X")(new Date(e))}}function t(){return function(e){return d3.format(".02f")(e)}}function i(){return function(e){return d3.format("f")(e)}}function a(){return function(e){return d3.format("%")(e)}}function n(){return function(e){return e.x}}function r(){return function(e){return e.y}}function s(){return"chart_"+Math.floor(65536*(1+Math.random())).toString(16).substring(1)}return{xAxisTickFormat:e,yAxisTickFormat:t,yAxisIntegerTickFormat:i,yAxisPercentageTickFormat:a,xFunction:n,yFunction:r,getId:s}}angular.module("app.services").factory("D3Service",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(e){this.name=e||null,this.data=[],this.subscribers=1};return a.prototype.toString=function(){return this.name},a.prototype.pushValue=function(t,i,a,n){var r,s,o=this;r=_.find(o.data,function(e){return e.iid===i}),angular.isDefined(r)&&null!==r?(r.values.push({x:t,y:n}),s=r.values.length-60*e.properties.window/e.properties.interval,s>0&&r.values.splice(0,s)):(r={key:angular.isDefined(a)?a:this.name,iid:i,values:[{x:t,y:n}]},o.data.push(r))},a.prototype.pushValues=function(t,a,n){var r,s,o=this;r=_.find(o.data,function(e){return e.iid===t}),angular.isDefined(r)&&null!==r?(r.values.push({x:a,y:n}),s=r.values.length-60*e.properties.window/e.properties.interval,s>0&&r.values.splice(0,s)):(r={key:"Series "+t,iid:t,values:[{x:a,y:n}]},o.data.push(r),i.getInames(o.name,t).then(function(e){angular.forEach(e.data.instances,function(e){e.instance===t&&(r.key=e.name)})}))},a.prototype.clearData=function(){this.data.length=0},a}e.$inject=["$rootScope","$log","MetricService"],angular.module("app.metrics").factory("SimpleMetric",e)}(),/**!
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
function(){"use strict";function e(e){var t=function(e,t){this.name=e,this.data=[],this.subscribers=1,this.derivedFunction=t};return t.prototype.updateValues=function(){var t,i=this;t=i.derivedFunction(),t.length!==i.data.length&&(i.data.length=0),angular.forEach(t,function(t){var a,n=_.find(i.data,function(e){return e.key===t.key});angular.isUndefined(n)?(n={key:t.key,values:[{x:t.timestamp,y:t.value}]},i.data.push(n)):(n.values.push({x:t.timestamp,y:t.value}),a=n.values.length-60*e.properties.window/e.properties.interval,a>0&&n.values.splice(0,a))})},t.prototype.clearData=function(){this.data.length=0},t}e.$inject=["$rootScope"],angular.module("app.metrics").factory("DerivedMetric",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){var n=function(e,t){this.base=n,this.base(e),this.conversionFunction=t};return n.prototype=new i,n.prototype.pushValue=function(t,i,a,n){var r,s,o,c,l=this;r=_.find(l.data,function(e){return e.iid===i}),angular.isUndefined(r)?(r={key:angular.isDefined(a)?a:this.name,iid:i,values:[],previousValue:n,previousTimestamp:t},l.data.push(r)):(o=(n-r.previousValue)/(t-r.previousTimestamp),c=l.conversionFunction(o),r.values.push({x:t,y:c}),r.previousValue=n,r.previousTimestamp=t,s=r.values.length-60*e.properties.window/e.properties.interval,s>0&&r.values.splice(0,s))},n.prototype.pushValues=function(t,i,n){var r,s,o,c,l=this;r=_.find(l.data,function(e){return e.iid===t}),angular.isUndefined(r)?(r={key:"Series "+t,iid:t,values:[],previousValue:n,previousTimestamp:i},l.data.push(r),a.getInames(l.name,t).then(function(e){angular.forEach(e.data.instances,function(e){e.instance===t&&(r.key=e.name)})})):(o=(n-r.previousValue)/(i-r.previousTimestamp),c=l.conversionFunction(o),r.values.push({x:i,y:c}),r.previousValue=n,r.previousTimestamp=i,s=r.values.length-60*e.properties.window/e.properties.interval,s>0&&r.values.splice(0,s))},n}e.$inject=["$rootScope","$log","SimpleMetric","MetricService"],angular.module("app.metrics").factory("CumulativeConvertedMetric",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){var n=function(e){this.base=i,this.base(e)};return n.prototype=new i,n.prototype.pushValue=function(t,i,a,n){var r,s,o,c=this;r=_.find(c.data,function(e){return e.iid===i}),angular.isUndefined(r)?(r={key:angular.isDefined(a)?a:this.name,iid:i,values:[],previousValue:n,previousTimestamp:t},c.data.push(r)):(o=(n-r.previousValue)/((t-r.previousTimestamp)/1e3),r.values.push({x:t,y:o}),r.previousValue=n,r.previousTimestamp=t,s=r.values.length-60*e.properties.window/e.properties.interval,s>0&&r.values.splice(0,s))},n.prototype.pushValues=function(t,i,n){var r,s,o,c=this;r=_.find(c.data,function(e){return e.iid===t}),angular.isUndefined(r)?(r={key:"Series "+t,iid:t,values:[],previousValue:n,previousTimestamp:i},c.data.push(r),a.getInames(c.name,t).then(function(e){angular.forEach(e.data.instances,function(e){e.instance===t&&(r.key=e.name)})})):(o=(n-r.previousValue)/((i-r.previousTimestamp)/1e3),r.values.push({x:i,y:o}),r.previousValue=n,r.previousTimestamp=i,s=r.values.length-60*e.properties.window/e.properties.interval,s>0&&r.values.splice(0,s))},n}e.$inject=["$rootScope","$log","SimpleMetric","MetricService"],angular.module("app.metrics").factory("CumulativeMetric",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){var n=function(e,t){this.base=i,this.base(e),this.conversionFunction=t};return n.prototype=new i,n.prototype.pushValue=function(t,i,a,n){var r,s,o,c=this;o=c.conversionFunction(n),r=_.find(c.data,function(e){return e.iid===i}),angular.isDefined(r)&&null!==r?(r.values.push({x:t,y:o}),s=r.values.length-60*e.properties.window/e.properties.interval,s>0&&r.values.splice(0,s)):(r={key:angular.isDefined(a)?a:this.name,iid:i,values:[{x:t,y:o}]},c.data.push(r))},n.prototype.pushValues=function(t,i,n){var r,s,o,c=this;o=c.conversionFunction(n),r=_.find(c.data,function(e){return e.iid===t}),angular.isDefined(r)&&null!==r?(r.values.push({x:i,y:o}),s=r.values.length-60*e.properties.window/e.properties.interval,s>0&&r.values.splice(0,s)):(r={key:"Series "+t,iid:t,values:[{x:i,y:o}]},c.data.push(r),a.getInames(c.name,t).then(function(e){angular.forEach(e.data.instances,function(e){e.instance===t&&(r.key=e.name)})}))},n}e.$inject=["$rootScope","$log","SimpleMetric","MetricService"],angular.module("app.metrics").factory("ConvertedMetric",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=t.getOrCreateCumulativeMetric("kernel.percpu.cpu.sys"),r=t.getOrCreateCumulativeMetric("kernel.percpu.cpu.user");a=function(){var e,t,i,a=[];return angular.forEach(n.data,function(n){n.values.length>0&&(e=_.find(r.data,function(e){return e.key===n.key}),angular.isDefined(e)&&(t=n.values[n.values.length-1],i=e.values[e.values.length-1],t.x===i.x&&a.push({timestamp:t.x,key:n.key,value:(t.y+i.y)/1e3})))}),a},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("kernel.percpu.cpu.sys"),t.destroyMetric("kernel.percpu.cpu.user"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("PerCpuUtilizationMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=t.getOrCreateCumulativeMetric("network.interface.in.bytes"),r=t.getOrCreateCumulativeMetric("network.interface.out.bytes");a=function(){var e,t=[];return angular.forEach(n.data,function(i){i.values.length>0&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:i.key+" in",value:e.y/1024}))}),angular.forEach(r.data,function(i){i.values.length>0&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:i.key+" out",value:e.y/1024}))}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("network.interface.in.bytes"),t.destroyMetric("network.interface.out.bytes"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("NetworkBytesMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metricDefinitions=this.dataModelOptions.metricDefinitions;var a,n={};angular.forEach(this.metricDefinitions,function(e,i){n[i]=t.getOrCreateMetric(e)}),a=function(){var e,t=[];return angular.forEach(n,function(i,a){angular.forEach(i.data,function(i){i.values.length>0&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:a.replace("{key}",i.key),value:e.y}))})}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),angular.forEach(this.metricDefinitions,function(e){t.destroyMetric(e)}),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("MultipleMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metricDefinitions=this.dataModelOptions.metricDefinitions;var a,n={};angular.forEach(this.metricDefinitions,function(e,i){n[i]=t.getOrCreateCumulativeMetric(e)}),a=function(){var e,t=[];return angular.forEach(n,function(i,a){angular.forEach(i.data,function(i){i.values.length>0&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:a.replace("{key}",i.key),value:e.y}))})}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),angular.forEach(this.metricDefinitions,function(e){t.destroyMetric(e)}),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("MultipleCumulativeMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=function(e){return e/1024},r=t.getOrCreateConvertedMetric("mem.util.cached",n),s=t.getOrCreateConvertedMetric("mem.util.used",n),o=t.getOrCreateConvertedMetric("mem.util.free",n),c=t.getOrCreateConvertedMetric("mem.util.bufmem",n);a=function(){var e,t,i,a,n=[];return e=function(){if(s.data.length>0){var e=s.data[s.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),t=function(){if(r.data.length>0){var e=r.data[r.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),i=function(){if(o.data.length>0){var e=o.data[o.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),a=function(){if(c.data.length>0){var e=c.data[c.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),angular.isDefined(e)&&angular.isDefined(t)&&angular.isDefined(a)&&n.push({timestamp:e.x,key:"application",value:e.y-t.y-a.y}),angular.isDefined(t)&&angular.isDefined(a)&&n.push({timestamp:e.x,key:"free (cache)",value:t.y+a.y}),angular.isDefined(i)&&n.push({timestamp:e.x,key:"free (unused)",value:i.y}),n},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("mem.util.cached"),t.destroyMetric("mem.util.used"),t.destroyMetric("mem.util.free"),t.destroyMetric("mem.util.bufmem"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("MemoryUtilizationMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=t.getOrCreateCumulativeMetric("disk.dev.read_rawactive"),r=t.getOrCreateCumulativeMetric("disk.dev.write_rawactive"),s=t.getOrCreateCumulativeMetric("disk.dev.read"),o=t.getOrCreateCumulativeMetric("disk.dev.write");a=function(){function e(e,n,r,s){e.data.length>0&&angular.forEach(e.data,function(e){t=_.find(n.data,function(t){return t.key===e.key}),angular.isDefined(t)&&e.values.length>0&&t.values.length>0&&(i=e.values[e.values.length-1],a=t.values[e.values.length-1],c=i.y>0?a.y/i.y:0,s.push({timestamp:i.x,key:e.key+r,value:c}))})}var t,i,a,c,l=[];return e(s,n," read latency",l),e(o,r," write latency",l),l},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("disk.dev.read_rawactive"),t.destroyMetric("disk.dev.write_rawactive"),t.destroyMetric("disk.dev.read"),t.destroyMetric("disk.dev.write"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("DiskLatencyMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=t.getOrCreateCumulativeMetric(this.name);a=function(){var e,t=[];return angular.forEach(n.data,function(i){i.values.length>0&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:i.key,value:e.y/1e3}))}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric(this.name),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("CumulativeUtilizationMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=t.getOrCreateCumulativeMetric("kernel.all.cpu.sys"),r=t.getOrCreateCumulativeMetric("kernel.all.cpu.user"),s=t.getOrCreateMetric("hinv.ncpu");a=function(){var e,t,i=[];return s.data.length>0&&(e=s.data[s.data.length-1],e.values.length>0&&(t=e.values[e.values.length-1].y,angular.forEach(n.data,function(e){if(e.values.length>0){var a=e.values[e.values.length-1];i.push({timestamp:a.x,key:"sys",value:a.y/(1e3*t)})}}),angular.forEach(r.data,function(e){if(e.values.length>0){var a=e.values[e.values.length-1];i.push({timestamp:a.x,key:"user",value:a.y/(1e3*t)})}}))),i},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("kernel.all.cpu.sys"),t.destroyMetric("kernel.all.cpu.user"),t.destroyMetric("hinv.ncpu"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","VectorService"],angular.module("app.datamodels").factory("CpuUtilizationMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a,n,r,s,o){function c(){e[0].hidden||e[0].webkitHidden||e[0].mozHidden||e[0].msHidden?o.cancelInterval():o.updateInterval()}function l(){o.initializeProperties(),n.host&&(d.inputHost=n.host,i.info("Host: "+n.host),n.hostspec&&(t.properties.hostspec=n.hostspec,i.info("Hostspec: "+n.hostspec)),o.updateHost(d.inputHost)),e[0].addEventListener("visibilitychange",c,!1),e[0].addEventListener("webkitvisibilitychange",c,!1),e[0].addEventListener("msvisibilitychange",c,!1),e[0].addEventListener("mozvisibilitychange",c,!1),i.info("Dashboard controller initialized with "+u+" view.")}var d=this,u=a.current.$$route.originalPath;d.dashboardOptions={hideToolbar:!0,widgetButtons:!1,hideWidgetName:!0,hideWidgetSettings:!0,widgetDefinitions:r,defaultWidgets:s},d.updateInterval=o.updateInterval,d.updateHost=function(){o.updateHost(d.inputHost)},d.updateWindow=o.updateWindow,d.isHostnameExpanded=!1,d.inputHost="",l()}e.$inject=["$document","$rootScope","$log","$route","$routeParams","widgetDefinitions","widgets","DashboardService"],angular.module("app.controllers",[]).controller("DashboardController",e)}(),/**!
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
function(){"use strict";function e(e,t){function i(i){i.yAxisTickFormat=t.yAxisTickFormat,i.xAxisTickFormat=t.xAxisTickFormat,i.yFunction=t.yFunction,i.xFunction=t.xFunction,i.id=t.getId(),i.height=250,i.flags=e.flags,i.$on("widgetResized",function(e,t){i.width=t.width||i.width,i.height=t.height||i.height,d3.select("#"+i.id+" svg").style({height:i.height})})}return{restrict:"A",templateUrl:"app/charts/line-timeseries.html",scope:{data:"="},link:i}}e.$inject=["$rootScope","D3Service"],angular.module("app.directives").directive("lineTimeSeries",e)}(),/**!
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
function(){"use strict";function e(e,t){function i(i){i.yAxisTickFormat=t.yAxisPercentageTickFormat,i.xAxisTickFormat=t.xAxisTickFormat,i.yFunction=t.yFunction,i.xFunction=t.xFunction,i.id=t.getId(),i.height=250,i.flags=e.flags,i.$on("widgetResized",function(e,t){i.width=t.width||i.width,i.height=t.height||i.height,d3.select("#"+i.id+" svg").style({height:i.height})})}return{restrict:"A",templateUrl:"app/charts/line-timeseries.html",scope:{data:"="},link:i}}e.$inject=["$rootScope","D3Service"],angular.module("app.directives").directive("linePercentageTimeSeries",e)}(),/**!
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
function(){"use strict";function e(e,t){function i(i){i.yAxisTickFormat=t.yAxisPercentageTickFormat,i.xAxisTickFormat=t.xAxisTickFormat,i.yFunction=t.yFunction,i.xFunction=t.xFunction,i.id=t.getId(),i.height=250,i.flags=e.flags,i.$on("widgetResized",function(e,t){i.width=t.width||i.width,i.height=t.height||i.height,d3.select("#"+i.id+" svg").style({height:i.height})})}return{restrict:"A",templateUrl:"app/charts/line-forcey-timeseries.html",scope:{data:"="},link:i}}e.$inject=["$rootScope","D3Service"],angular.module("app.directives").directive("linePercentageForceYTimeSeries",e)}(),/**!
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
function(){"use strict";function e(e,t){function i(i){i.yAxisTickFormat=t.yAxisIntegerTickFormat,i.xAxisTickFormat=t.xAxisTickFormat,i.yFunction=t.yFunction,i.xFunction=t.xFunction,i.id=t.getId(),i.height=250,i.flags=e.flags,i.$on("widgetResized",function(e,t){i.width=t.width||i.width,i.height=t.height||i.height,d3.select("#"+i.id+" svg").style({height:i.height})})}return{restrict:"A",templateUrl:"app/charts/line-timeseries.html",scope:{data:"="},link:i}}e.$inject=["$rootScope","D3Service"],angular.module("app.directives").directive("lineIntegerTimeSeries",e)}(),/**!
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
function(){"use strict";function e(e,t){function i(i){i.yAxisTickFormat=t.yAxisIntegerTickFormat,i.xAxisTickFormat=t.xAxisTickFormat,i.yFunction=t.yFunction,i.xFunction=t.xFunction,i.id=t.getId(),i.height=250,i.flags=e.flags,i.$on("widgetResized",function(e,t){i.width=t.width||i.width,i.height=t.height||i.height,d3.select("#"+i.id+" svg").style({height:i.height})})}return{restrict:"A",templateUrl:"app/charts/line-integer-forcey-timeseries.html",scope:{data:"="},link:i}}e.$inject=["$rootScope","D3Service"],angular.module("app.directives").directive("lineIntegerForceYTimeSeries",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){function n(n){n.host=e.properties.host,n.port=e.properties.port,n.context=e.properties.context,n.ready=!1,n.processing=!1,n.id=a.getGuid(),n.generateHeatMap=function(){i.generate(),n.ready=!0,n.processing=!0,t(function(){n.processing=!1},15e4)}}return{restrict:"A",templateUrl:"app/charts/disk-latency-heatmap.html",link:n}}e.$inject=["$rootScope","$timeout","HeatMapService","VectorService"],angular.module("app.directives").directive("diskLatencyHeatMap",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){function n(n){n.host=e.properties.host,n.port=e.properties.port,n.context=e.properties.context,n.ready=!1,n.processing=!1,n.id=a.getGuid(),n.generateFlameGraph=function(){i.generate(),n.ready=!0,n.processing=!0,t(function(){n.processing=!1},65e3)}}return{restrict:"A",templateUrl:"app/charts/pu-flame-graph.html",link:n}}e.$inject=["$rootScope","$timeout","FlameGraphService","VectorService"],angular.module("app.directives").directive("cpuFlameGraph",e)}(),/**!
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
function(){"use strict";function e(e,t){function i(i){i.yAxisTickFormat=t.yAxisTickFormat,i.xAxisTickFormat=t.xAxisTickFormat,i.yFunction=t.yFunction,i.xFunction=t.xFunction,i.id=t.getId(),i.height=250,i.flags=e.flags,i.$on("widgetResized",function(e,t){i.width=t.width||i.width,i.height=t.height||i.height,d3.select("#"+i.id+" svg").style({height:i.height})})}return{restrict:"A",templateUrl:"app/charts/area-stacked-timeseries.html",scope:{data:"="},link:i}}e.$inject=["$rootScope","D3Service"],angular.module("app.directives").directive("areaStackedTimeSeries",e)}(),/**!
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
function(){"use strict";function e(e,t){function i(i){i.yAxisTickFormat=t.yAxisPercentageTickFormat,i.xAxisTickFormat=t.xAxisTickFormat,i.yFunction=t.yFunction,i.xFunction=t.xFunction,i.id=t.getId(),i.height=250,i.flags=e.flags,i.$on("widgetResized",function(e,t){i.width=t.width||i.width,i.height=t.height||i.height,d3.select("#"+i.id+" svg").style({height:i.height})})}return{restrict:"A",templateUrl:"app/charts/area-stacked-timeseries.html",scope:{data:"="},link:i}}e.$inject=["$rootScope","D3Service"],angular.module("app.directives").directive("areaStackedPercentageTimeSeries",e)}(),/**!
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
function(){"use strict";function e(e,t){function i(i){i.yAxisTickFormat=t.yAxisPercentageTickFormat,i.xAxisTickFormat=t.xAxisTickFormat,i.yFunction=t.yFunction,i.xFunction=t.xFunction,i.id=t.getId(),i.height=250,i.flags=e.flags,i.$on("widgetResized",function(e,t){i.width=t.width||i.width,i.height=t.height||i.height,d3.select("#"+i.id+" svg").style({height:i.height})})}return{restrict:"A",templateUrl:"app/charts/area-stacked-forcey-timeseries.html",scope:{data:"="},link:i}}e.$inject=["$rootScope","D3Service"],angular.module("app.directives").directive("areaStackedPercentageForceYTimeSeries",e)}(),/**!
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
function(){"use strict";function e(e,t){function i(i){i.yAxisTickFormat=t.yAxisIntegerTickFormat,i.xAxisTickFormat=t.xAxisTickFormat,i.yFunction=t.yFunction,i.xFunction=t.xFunction,i.id=t.getId(),i.height=250,i.flags=e.flags,i.$on("widgetResized",function(e,t){i.width=t.width||i.width,i.height=t.height||i.height,d3.select("#"+i.id+" svg").style({height:i.height})})}return{restrict:"A",templateUrl:"app/charts/area-stacked-timeseries.html",scope:{data:"="},link:i}}e.$inject=["$rootScope","D3Service"],angular.module("app.directives").directive("areaStackedIntegerTimeSeries",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a,n,r,s,o,c,l,d,u){var p=[{name:"kernel.all.load",title:"Load Average",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"kernel.all.load"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"},{name:"kernel.all.runnable",title:"Runnable",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"kernel.all.runnable"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"},{name:"kernel.all.cpu.sys",title:"CPU Utilization (System)",directive:"line-percentage-force-y-time-series",dataAttrName:"data",dataModelType:d,dataModelOptions:{name:"kernel.all.cpu.sys"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"},{name:"kernel.all.cpu.user",title:"CPU Utilization (User)",directive:"line-percentage-force-y-time-series",dataAttrName:"data",dataModelType:d,dataModelOptions:{name:"kernel.all.cpu.user"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"},{name:"kernel.all.cpu",title:"CPU Utilization",directive:"area-stacked-percentage-force-y-time-series",dataAttrName:"data",dataModelType:n,dataModelOptions:{name:"kernel.all.cpu"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"},{name:"kernel.percpu.cpu.sys",title:"Per-CPU Utilization (System)",directive:"line-percentage-force-y-time-series",dataAttrName:"data",dataModelType:d,dataModelOptions:{name:"kernel.percpu.cpu.sys"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"},{name:"kernel.percpu.cpu.user",title:"Per-CPU Utilization (User)",directive:"line-percentage-force-y-time-series",dataAttrName:"data",dataModelType:d,dataModelOptions:{name:"kernel.percpu.cpu.user"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"},{name:"kernel.percpu.cpu",title:"Per-CPU Utilization",directive:"line-percentage-force-y-time-series",dataAttrName:"data",dataModelType:r,dataModelOptions:{name:"kernel.percpu.cpu"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"},{name:"mem.freemem",title:"Memory Utilization (Free)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"mem.freemem"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Memory"},{name:"mem.util.used",title:"Memory Utilization (Used)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"mem.util.used"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Memory"},{name:"mem.util.cached",title:"Memory Utilization (Cached)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"mem.util.cached"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Memory"},{name:"mem",title:"Memory Utilization",directive:"area-stacked-integer-time-series",dataAttrName:"data",dataModelType:i,dataModelOptions:{name:"mem"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Memory"},{name:"network.interface.out.drops",title:"Network Drops (Out)",directive:"line-integer-force-y-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.interface.out.drops"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network"},{name:"network.interface.in.drops",title:"Network Drops (In)",directive:"line-integer-force-y-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.interface.in.drops"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network"},{name:"network.interface.drops",title:"Network Drops",directive:"line-integer-force-y-time-series",dataAttrName:"data",dataModelType:s,dataModelOptions:{name:"network.interface.drops",metricDefinitions:{"{key} in":"network.interface.in.drops","{key} out":"network.interface.out.drops"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network"},{name:"network.tcpconn.established",title:"TCP Connections (Estabilished)",directive:"line-integer-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.tcpconn.established"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network"},{name:"network.tcpconn.time_wait",title:"TCP Connections (Time Wait)",directive:"line-integer-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.tcpconn.time_wait"},enableVerticalResize:!1,size:{width:"25%",height:"250px"},group:"Network"},{name:"network.tcpconn.close_wait",title:"TCP Connections (Close Wait)",directive:"line-integer-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.tcpconn.close_wait"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network"},{name:"network.tcpconn",title:"TCP Connections",directive:"line-integer-time-series",dataAttrName:"data",dataModelType:s,dataModelOptions:{name:"network.tcpconn",metricDefinitions:{established:"network.tcpconn.established",time_wait:"network.tcpconn.time_wait",close_wait:"network.tcpconn.close_wait"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network"},{name:"network.interface.bytes",title:"Network Throughput (kB)",directive:"line-integer-time-series",dataAttrName:"data",dataModelType:a,dataModelOptions:{name:"network.interface.bytes"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network"},{name:"disk.iops",title:"Disk IOPS",directive:"line-time-series",dataAttrName:"data",dataModelType:o,dataModelOptions:{name:"disk.iops",metricDefinitions:{"{key} read":"disk.dev.read","{key} write":"disk.dev.write"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Disk"},{name:"disk.bytes",title:"Disk Throughput (Bytes)",directive:"line-time-series",dataAttrName:"data",dataModelType:o,dataModelOptions:{name:"disk.bytes",metricDefinitions:{"{key} read":"disk.dev.read_bytes","{key} write":"disk.dev.write_bytes"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Disk"},{name:"disk.dev.avactive",title:"Disk Utilization",directive:"line-percentage-force-y-time-series",dataAttrName:"data",dataModelType:d,dataModelOptions:{name:"disk.dev.avactive"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Disk"},{name:"kernel.all.pswitch",title:"Context Switches",directive:"line-integer-time-series",dataAttrName:"data",dataModelType:t,dataModelOptions:{name:"kernel.all.pswitch"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"},{name:"mem.vmstat.pgfault",title:"Page Faults",directive:"area-stacked-integer-time-series",dataAttrName:"data",dataModelType:o,dataModelOptions:{name:"mem.vmstat.pgfault",metricDefinitions:{"page faults":"mem.vmstat.pgfault","major page faults":"mem.vmstat.pgmajfault"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Memory"},{name:"network.interface.packets",title:"Network Packets",directive:"line-integer-time-series",dataAttrName:"data",dataModelType:o,dataModelOptions:{name:"network.interface.packets",metricDefinitions:{"{key} in":"network.interface.in.packets","{key} out":"network.interface.out.packets"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network"},{name:"network.tcp.retrans",title:"Network Retransmits",directive:"line-integer-force-y-time-series",dataAttrName:"data",dataModelType:o,dataModelOptions:{name:"network.tcp.retrans",metricDefinitions:{retranssegs:"network.tcp.retranssegs",timeouts:"network.tcp.timeouts",listendrops:"network.tcp.listendrops",fastretrans:"network.tcp.fastretrans",slowstartretrans:"network.tcp.slowstartretrans",syncretrans:"network.tcp.syncretrans"}},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Network"},{name:"disk.dev.latency",title:"Disk Latency",directive:"line-integer-time-series",dataAttrName:"data",dataModelType:l,dataModelOptions:{name:"disk.dev.latency"},size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Disk"}];return u.enableCpuFlameGraph&&p.push({name:"graph.flame.cpu",title:"CPU Flame Graph",directive:"cpu-flame-graph",dataModelType:c,size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"CPU"}),u.enableDiskLatencyHeatMap&&p.push({name:"graph.heatmap.disk",title:"Disk Latency Heat Map",directive:"disk-latency-heat-map",dataModelType:c,size:{width:"25%",height:"250px"},enableVerticalResize:!1,group:"Disk"}),p}e.$inject=["MetricDataModel","CumulativeMetricDataModel","MemoryUtilizationMetricDataModel","NetworkBytesMetricDataModel","CpuUtilizationMetricDataModel","PerCpuUtilizationMetricDataModel","MultipleMetricDataModel","MultipleCumulativeMetricDataModel","DummyMetricDataModel","DiskLatencyMetricDataModel","CumulativeUtilizationMetricDataModel","vectorConfig"];var t=[{name:"kernel.all.cpu",size:{width:"25%"}},{name:"kernel.percpu.cpu",size:{width:"25%"}},{name:"kernel.all.runnable",size:{width:"25%"}},{name:"kernel.all.load",size:{width:"25%"}},{name:"network.interface.bytes",size:{width:"25%"}},{name:"network.tcpconn",size:{width:"25%"}},{name:"network.interface.packets",size:{width:"25%"}},{name:"network.tcp.retrans",size:{width:"25%"}},{name:"mem",size:{width:"50%"}},{name:"mem.vmstat.pgfault",size:{width:"25%"}},{name:"kernel.all.pswitch",size:{width:"25%"}},{name:"disk.iops",size:{width:"25%"}},{name:"disk.bytes",size:{width:"25%"}},{name:"disk.dev.avactive",size:{width:"25%"}},{name:"disk.dev.latency",size:{width:"25%"}}],i=[];angular.module("app.widgets",[]).factory("widgetDefinitions",e).value("defaultWidgets",t).value("emptyWidgets",i)}(),/**!
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
function(){"use strict";function e(e){e.when("/",{templateUrl:"app/dashboard/dashboard.html",controller:"DashboardController",controllerAs:"vm",title:"Dashboard - Vector",reloadOnSearch:!1,resolve:{widgets:["defaultWidgets",function(e){return e}]}}).when("/empty",{templateUrl:"app/dashboard/dashboard.html",controller:"DashboardController",controllerAs:"vm",title:"Dashboard - Vector",reloadOnSearch:!1,resolve:{widgets:["emptyWidgets",function(e){return e}]}}).otherwise("/")}e.$inject=["$routeProvider"],angular.module("app.routes").config(e)}(),/**!
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
function(){"use strict";function e(e,t){var i,a,n=[];for(i=0;i<e.length;i++)a=e[i][t],-1===n.indexOf(a)&&n.push(a);return n}function t(){return function(t,i){return null!==t?e(t,i):void 0}}function i(){return function(e,t){return e.filter(function(e){return e.group===t})}}angular.module("app.filters").filter("groupBy",t).filter("groupFilter",i)}(),/**!
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
function(){"use strict";angular.module("vector.config").constant("vectorConfig",{port:44323,hostspec:"localhost",interval:2,window:2,enableCpuFlameGraph:!1,enableDiskLatencyHeatMap:!1})}(),angular.module("app").run(["$templateCache",function(e){e.put("app/charts/area-stacked-forcey-timeseries.html",'<div><nvd3-stacked-area-chart data="data" id="{{id}}" xaxistickformat="xAxisTickFormat()" yaxistickformat="yAxisTickFormat()" height="250" margin="{left: 50, right: 50}" useinteractiveguideline="true" transitionduration="0" showlegend="true" showxaxis="true" showyaxis="true" interactive="true" tooltips="true" showcontrols="false" stacked="true" nodata="No available data." x="xFunction()" y="yFunction()" objectequality="true" ydomain="[0,1]" ng-if="flags.contextAvailable"><svg></svg></nvd3-stacked-area-chart><i class="fa fa-line-chart fa-5x widget-icon" ng-if="!flags.contextAvailable"></i></div>'),e.put("app/charts/area-stacked-timeseries.html",'<div><nvd3-stacked-area-chart data="data" id="{{id}}" xaxistickformat="xAxisTickFormat()" yaxistickformat="yAxisTickFormat()" height="250" margin="{left: 50, right: 50}" useinteractiveguideline="true" transitionduration="0" showlegend="true" showxaxis="true" showyaxis="true" interactive="true" tooltips="true" showcontrols="false" stacked="true" nodata="No available data." x="xFunction()" y="yFunction()" objectequality="true" ng-if="flags.contextAvailable"><svg></svg></nvd3-stacked-area-chart><i class="fa fa-line-chart fa-5x widget-icon" ng-if="!flags.contextAvailable"></i></div>'),e.put("app/charts/cpu-flame-graph.html",'<div class="col-md-12"><div class="row" style="text-align: center;"><p class="lead" ng-if="!ready">Click on the button below to generate your CPU Flame Graph! (60 sec)</p></div><div class="row" ng-if="!processing && ready" style="text-align: center;"><object data="http://{{host}}:{{port}}/systack/systack.svg" type="image/svg+xml" style="width:100%; height:auto;"></object><br><a class="btn btn-default pull-right" href="http://{{host}}:{{port}}/systack/systack.svg" target="_blank">Open in a new Tab</a></div><div class="row"><div id="alert-sysstack-error" flash-alert="error" active-class="in alert" class="fade"><strong class="alert-heading">Error!</strong> <span class="alert-message">{{flash.message}}</span></div></div><div class="row"><div id="alert-sysstack-success" flash-alert="success" active-class="in alert" class="fade"><strong class="alert-heading">Success!</strong> <span class="alert-message">{{flash.message}}</span></div></div><div class="row" style="text-align: center; margin-top: 15px;"><button type="button" class="btn btn-primary btn-lg" ng-disabled="processing" ng-click="generateFlameGraph()">Generate <i ng-if="processing" class="fa fa-refresh fa-spin"></i></button></div></div>'),e.put("app/charts/disk-latency-heatmap.html",'<div class="col-md-12"><div class="row" style="text-align: center;"><p class="lead" ng-if="!ready">Click on the button below to generate your Disk Latency Heat Map! (140 sec)</p></div><div class="row" ng-if="!processing && ready" style="text-align: center;"><object data="http://{{host}}:{{port}}/heatmap/heatmap.svg" type="image/svg+xml" style="width:100%; height:auto;"></object><br><a class="btn btn-default pull-right" href="http://{{host}}:{{port}}/heatmap/heatmap.svg" target="_blank">Open in a new Tab</a></div><div class="row"><div id="alert-disklatency-error" flash-alert="error" active-class="in alert" class="fade"><strong class="alert-heading">Error!</strong> <span class="alert-message">{{flash.message}}</span></div></div><div class="row"><div id="alert-disklatency-success" flash-alert="success" active-class="in alert" class="fade"><strong class="alert-heading">Success!</strong> <span class="alert-message">{{flash.message}}</span></div></div><div class="row" style="text-align: center; margin-top: 15px;"><button type="button" class="btn btn-primary btn-lg" ng-disabled="processing" ng-click="generateHeatMap()">Generate <i ng-if="processing" class="fa fa-refresh fa-spin"></i></button></div></div>'),e.put("app/charts/line-forcey-timeseries.html",'<div><nvd3-line-chart data="data" id="{{id}}" xaxistickformat="xAxisTickFormat()" yaxistickformat="yAxisTickFormat()" height="250" margin="{left: 50, right: 50}" useinteractiveguideline="true" transitionduration="0" showlegend="true" showxaxis="true" showyaxis="true" interactive="true" tooltips="true" nodata="No available data." x="xFunction()" y="yFunction()" objectequality="true" forcey="[0,1]" ng-if="flags.contextAvailable"><svg></svg></nvd3-line-chart><i class="fa fa-line-chart fa-5x widget-icon" ng-if="!flags.contextAvailable"></i></div>'),e.put("app/charts/line-integer-forcey-timeseries.html",'<div><nvd3-line-chart data="data" id="{{id}}" xaxistickformat="xAxisTickFormat()" yaxistickformat="yAxisTickFormat()" height="250" margin="{left: 50, right: 50}" useinteractiveguideline="true" transitionduration="0" showlegend="true" showxaxis="true" showyaxis="true" interactive="true" tooltips="true" nodata="No available data." x="xFunction()" y="yFunction()" objectequality="true" forcey="[0,10]" ng-if="flags.contextAvailable"><svg></svg></nvd3-line-chart><i class="fa fa-line-chart fa-5x widget-icon" ng-if="!flags.contextAvailable"></i></div>'),e.put("app/charts/line-timeseries.html",'<div><nvd3-line-chart data="data" id="{{id}}" xaxistickformat="xAxisTickFormat()" yaxistickformat="yAxisTickFormat()" height="{{height}}" margin="{left: 50, right: 50}" useinteractiveguideline="true" transitionduration="0" showlegend="true" showxaxis="true" showyaxis="true" interactive="true" tooltips="true" nodata="No available data." x="xFunction()" y="yFunction()" objectequality="true" ,="" ng-if="flags.contextAvailable"><svg></svg></nvd3-line-chart><i class="fa fa-line-chart fa-5x widget-icon" ng-if="!flags.contextAvailable"></i></div>'),e.put("app/dashboard/dashboard.html",'<div class="row"><div class="col-md-12"><div dashboard="vm.dashboardOptions" template-url="app/dashboard/vector.dashboard.html" class="dashboard-container"></div></div></div>'),e.put("app/dashboard/vector.dashboard.html",'<div class="row"><div class="col-md-6"><form role="form" name="form"><div class="input-group input-group-lg" ng-class="{\'has-error\': form.host.$invalid && form.host.$dirty}"><span class="input-group-addon" ng-click="vm.isHostnameExpanded = !vm.isHostnameExpanded">Hostname &nbsp; <i class="fa fa-plus-square-o"></i></span> <input type="text" class="form-control" id="hostnameInput" name="host" data-content="Please enter the instance hostname. Port can be specified using the <hostname>:<port> format. Expand to change hostspec." rel="popover" data-placement="bottom" data-trigger="hover" data-container="body" ng-model="vm.inputHost" ng-change="vm.updateHost()" ng-model-options="{ updateOn: \'default\', debounce: 1000 }" delay="1000" ng-disabled="flags.contextUpdating == true" required="" placeholder="Instance Hostname"> <i class="fa fa-refresh fa-2x form-control-feedback" ng-if="flags.contextUpdating"></i> <i class="fa fa-check fa-2x form-control-feedback" ng-if="flags.contextAvailable"></i></div></form></div><div class="btn-group col-md-2"><button id="widgetButton" type="button" class="dropdown-toggle btn btn-lg btn-default btn-block" data-toggle="dropdown">Widget <span class="caret"></span></button><ul class="dropdown-menu" role="menu"><li class="dropdown-submenu" ng-repeat="group in widgetDefs | groupBy: \'group\'"><a ng-click="void(0)" data-toggle="dropdown">{{group}}</a><ul class="dropdown-menu"><li ng-repeat="widget in widgetDefs | groupFilter: group"><a href="#" ng-click="addWidgetInternal($event, widget);">{{widget.title}}</a></li></ul></li><li role="presentation" class="divider"></li><li><a href="javascript:void(0);" ng-click="loadWidgets(defaultWidgets);">Default Widgets</a></li><li><a href="javascript:void(0);" ng-click="loadWidgets(emptyWidgets);">Clear</a></li></ul></div><div class="col-md-2"><form role="form"><div class="input-group input-group-lg" ng-class="{\'has-error\': form.window.$invalid && form.window.$dirty}" style="border-radius: 6 !important;"><span class="input-group-addon">Window</span><select class="form-control" name="window" id="windowInput" data-content="The duration window for all charts in this dashboard." rel="popover" data-placement="bottom" data-trigger="hover" data-container="body" ng-model="properties.window" ng-change="vm.updateWindow()" style="border-radius: 6 !important;"><option value="2">2 min</option><option value="5">5 min</option><option value="10">10 min</option></select></div></form></div><div class="col-md-2"><form role="form"><div class="input-group input-group-lg" ng-class="{\'has-error\': form.interval.$invalid && form.interval.$dirty}"><span class="input-group-addon">Interval</span><select class="form-control" name="interval" id="intervalInput" data-content="The update interval used by all charts in this dashboard." rel="popover" data-placement="bottom" data-trigger="hover" data-container="body" ng-model="properties.interval" ng-change="vm.updateInterval()"><option value="1">1 sec</option><option value="2">2 sec</option><option value="3">3 sec</option><option value="5">5 sec</option></select></div></form></div></div><div class="row" ng-show="vm.isHostnameExpanded"><div class="col-md-6"><form role="form" name="form"><div class="input-group input-group-lg" ng-class="{\'has-error\': form.hostspec.$invalid && form.hostspec.$dirty}"><span class="input-group-addon">Hostspec&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span> <input type="text" class="form-control" id="hostspecInput" name="host" data-content="Please enter the instance hostspec." rel="popover" data-placement="bottom" data-trigger="hover" data-container="body" ng-model="properties.hostspec" ng-change="vm.updateHost()" ng-model-options="{ updateOn: \'default\', debounce: 1000 }" delay="1000" ng-disabled="flags.contextUpdating == true" required="" placeholder="Instance Hostspec"></div></form></div><div class="col-md-2"><p class="lead" id="hostnameLabel" data-content="PMCD hostname. The hostname from the actual instance you\'re monitoring." rel="popover" data-placement="bottom" data-trigger="hover" data-container="body">{{flags.contextAvailable && properties.hostname ? properties.hostname : \'Disconnected\'}}</p></div></div><div id="alert-dashboard-error" flash-alert="error" active-class="in alert" class="fade"><strong class="alert-heading">Error!</strong> <span class="alert-message">{{flash.message}}</span></div><div class="row"><div class="col-md-12"><div class="btn-toolbar" ng-if="!options.hideToolbar"><div class="btn-group" ng-if="!options.widgetButtons"><button type="button" class="dropdown-toggle btn btn-primary" data-toggle="dropdown">Add Widget <span class="caret"></span></button><ul class="dropdown-menu" role="menu"><li ng-repeat="widget in widgetDefs"><a href="#" ng-click="addWidgetInternal($event, widget);"><span class="label label-primary">{{widget.name}}</span></a></li></ul></div><div class="btn-group" ng-if="options.widgetButtons"><button ng-repeat="widget in widgetDefs" ng-click="addWidgetInternal($event, widget);" type="button" class="btn btn-primary">{{widget.name}}</button></div><button class="btn btn-warning" ng-click="resetWidgetsToDefault()">Default Widgets</button> <button ng-if="options.storage && options.explicitSave" ng-click="options.saveDashboard()" class="btn btn-success" ng-disabled="!options.unsavedChangeCount">{{ !options.unsavedChangeCount ? "all saved" : "save changes (" + options.unsavedChangeCount + ")" }}</button> <button ng-click="clear();" type="button" class="btn btn-info">Clear</button></div><div ui-sortable="sortableOptions" ng-model="widgets" class="dashboard-widget-area"><div ng-repeat="widget in widgets" ng-style="widget.containerStyle" class="widget-container" widget=""><div class="widget panel panel-default"><div class="widget-header panel-heading"><h3 class="panel-title"><span class="widget-title" ng-dblclick="editTitle(widget)" ng-hide="widget.editingTitle">{{widget.title}}</span><span class="label label-primary" ng-if="!options.hideWidgetName">{{widget.name}}</span> <span ng-click="removeWidget(widget);" class="glyphicon glyphicon-remove" ng-if="!options.hideWidgetClose"></span> <span ng-click="openWidgetDialog(widget);" class="glyphicon glyphicon-cog" ng-if="!options.hideWidgetSettings"></span></h3></div><div class="panel-body widget-content" ng-style="widget.contentStyle"></div><div class="widget-ew-resizer" ng-mousedown="grabResizer($event)"></div><div ng-if="widget.enableVerticalResize" class="widget-s-resizer" ng-mousedown="grabSouthResizer($event)"></div></div></div></div></div></div><script>\n    (function () {\n        \'use strict\';\n        $(\'#hostnameInput\').popover();\n        $(\'#hostspecInput\').popover();\n        $(\'#windowInput\').popover();\n        $(\'#intervalInput\').popover();\n        $(\'#hostnameLabel\').popover();\n    }());\n</script>')}]);