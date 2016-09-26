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
!function(){"use strict";function e(e,t,i,a,n,r,o,s,c,l,d,u,p,m,g,h,f,v,y,M,b,k){var w=function(e,t){"undefined"!=typeof e&&(t.filter=e.filter)},C=[{name:"kernel.all.load",title:"Load Average",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"kernel.all.load"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"CPU"},{name:"kernel.all.runnable",title:"Runnable",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"kernel.all.runnable"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:4,integer:!0}},{name:"kernel.all.cpu.sys",title:"CPU Utilization (System)",directive:"line-time-series",dataAttrName:"data",dataModelType:M,dataModelOptions:{name:"kernel.all.cpu.sys"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.all.cpu.user",title:"CPU Utilization (User)",directive:"line-time-series",dataAttrName:"data",dataModelType:M,dataModelOptions:{name:"kernel.all.cpu.user"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.all.cpu",title:"CPU Utilization",directive:"area-stacked-time-series",dataAttrName:"data",dataModelType:m,dataModelOptions:{name:"kernel.all.cpu"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.percpu.cpu.sys",title:"Per-CPU Utilization (System)",directive:"line-time-series",dataAttrName:"data",dataModelType:M,dataModelOptions:{name:"kernel.percpu.cpu.sys"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.percpu.cpu.user",title:"Per-CPU Utilization (User)",directive:"line-time-series",dataAttrName:"data",dataModelType:M,dataModelOptions:{name:"kernel.percpu.cpu.user"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.percpu.cpu",title:"Per-CPU Utilization",directive:"line-time-series",dataAttrName:"data",dataModelType:g,dataModelOptions:{name:"kernel.percpu.cpu"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"mem.util.free",title:"Memory Utilization (Free)",directive:"line-time-series",dataAttrName:"data",dataModelType:t,dataModelOptions:{name:"mem.util.free",conversionFunction:function(e){return e/1024/1024}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Memory"},{name:"mem.util.used",title:"Memory Utilization (Used)",directive:"line-time-series",dataAttrName:"data",dataModelType:t,dataModelOptions:{name:"mem.util.used",conversionFunction:function(e){return e/1024/1024}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Memory"},{name:"mem.util.cached",title:"Memory Utilization (Cached)",directive:"line-time-series",dataAttrName:"data",dataModelType:t,dataModelOptions:{name:"mem.util.cached",conversionFunction:function(e){return e/1024/1024}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Memory"},{name:"mem",title:"Memory Utilization",directive:"area-stacked-time-series",dataAttrName:"data",dataModelType:u,dataModelOptions:{name:"mem"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Memory",attrs:{percentage:!1,integer:!0}},{name:"network.interface.out.drops",title:"Network Drops (Out)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.interface.out.drops"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{forcey:10,percentage:!1,integer:!0}},{name:"network.interface.in.drops",title:"Network Drops (In)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.interface.in.drops"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{forcey:10,percentage:!1,integer:!0}},{name:"network.interface.drops",title:"Network Drops",directive:"line-time-series",dataAttrName:"data",dataModelType:h,dataModelOptions:{name:"network.interface.drops",metricDefinitions:{"{key} in":"network.interface.in.drops","{key} out":"network.interface.out.drops"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{forcey:10,percentage:!1,integer:!0}},{name:"network.tcpconn.established",title:"TCP Connections (Estabilished)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.tcpconn.established"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{percentage:!1,integer:!0}},{name:"network.tcpconn.time_wait",title:"TCP Connections (Time Wait)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.tcpconn.time_wait"},enableVerticalResize:!1,size:{width:"50%",height:"250px"},group:"Network",attrs:{percentage:!1,integer:!0}},{name:"network.tcpconn.close_wait",title:"TCP Connections (Close Wait)",directive:"line-time-series",dataAttrName:"data",dataModelType:e,dataModelOptions:{name:"network.tcpconn.close_wait"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{percentage:!1,integer:!0}},{name:"network.tcpconn",title:"TCP Connections",directive:"line-time-series",dataAttrName:"data",dataModelType:h,dataModelOptions:{name:"network.tcpconn",metricDefinitions:{established:"network.tcpconn.established",time_wait:"network.tcpconn.time_wait",close_wait:"network.tcpconn.close_wait"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{percentage:!1,integer:!0}},{name:"network.interface.bytes",title:"Network Throughput (kB)",directive:"line-time-series",dataAttrName:"data",dataModelType:p,dataModelOptions:{name:"network.interface.bytes"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{percentage:!1,integer:!0},settingsModalOptions:{templateUrl:"app/components/customWidgetSettings/customWidgetSettings.html",controller:"CustomWidgetSettingsController"},hasLocalSettings:!0,onSettingsClose:w,filter:""},{name:"disk.iops",title:"Disk IOPS",directive:"line-time-series",dataAttrName:"data",dataModelType:f,dataModelOptions:{name:"disk.iops",metricDefinitions:{"{key} read":"disk.dev.read","{key} write":"disk.dev.write"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Disk"},{name:"disk.bytes",title:"Disk Throughput (Bytes)",directive:"line-time-series",dataAttrName:"data",dataModelType:f,dataModelOptions:{name:"disk.bytes",metricDefinitions:{"{key} read":"disk.dev.read_bytes","{key} write":"disk.dev.write_bytes"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Disk"},{name:"disk.dev.avactive",title:"Disk Utilization",directive:"line-time-series",dataAttrName:"data",dataModelType:M,dataModelOptions:{name:"disk.dev.avactive"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Disk",attrs:{forcey:1,percentage:!0,integer:!1}},{name:"kernel.all.pswitch",title:"Context Switches",directive:"line-time-series",dataAttrName:"data",dataModelType:i,dataModelOptions:{name:"kernel.all.pswitch"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"CPU",attrs:{percentage:!1,integer:!0}},{name:"mem.vmstat.pgfault",title:"Page Faults",directive:"area-stacked-time-series",dataAttrName:"data",dataModelType:f,dataModelOptions:{name:"mem.vmstat.pgfault",metricDefinitions:{"page faults":"mem.vmstat.pgfault","major page faults":"mem.vmstat.pgmajfault"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Memory",attrs:{percentage:!1,integer:!0}},{name:"network.interface.packets",title:"Network Packets",directive:"line-time-series",dataAttrName:"data",dataModelType:f,dataModelOptions:{name:"network.interface.packets",metricDefinitions:{"{key} in":"network.interface.in.packets","{key} out":"network.interface.out.packets"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{percentage:!1,integer:!0},settingsModalOptions:{templateUrl:"app/components/customWidgetSettings/customWidgetSettings.html",controller:"CustomWidgetSettingsController"},hasLocalSettings:!0,onSettingsClose:w,filter:""},{name:"network.tcp.retrans",title:"Network Retransmits",directive:"line-time-series",dataAttrName:"data",dataModelType:f,dataModelOptions:{name:"network.tcp.retrans",metricDefinitions:{retranssegs:"network.tcp.retranssegs",timeouts:"network.tcp.timeouts",listendrops:"network.tcp.listendrops",fastretrans:"network.tcp.fastretrans",slowstartretrans:"network.tcp.slowstartretrans",syncretrans:"network.tcp.syncretrans"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Network",attrs:{forcey:10,percentage:!1,integer:!0}},{name:"disk.dev.latency",title:"Disk Latency",directive:"line-time-series",dataAttrName:"data",dataModelType:y,dataModelOptions:{name:"disk.dev.latency"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Disk",attrs:{percentage:!1,integer:!0}}];return k.enableCpuFlameGraph&&C.push({name:"graph.flame.cpu",title:"CPU Flame Graph",directive:"cpu-flame-graph",dataModelType:v,size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"CPU"}),k.enableContainerWidgets&&C.push({name:"cgroup.cpuacct.usage",title:"Per-Container CPU Utilization",directive:"line-time-series",dataAttrName:"data",dataModelType:a,dataModelOptions:{name:"cgroup.cpuacct.usage"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",requireContainerFilter:!0,attrs:{forcey:1,percentage:!0,integer:!1}},{name:"cgroup.memory.usage",title:"Per-Container Memory Usage (Mb)",directive:"line-time-series",dataAttrName:"data",dataModelType:r,dataModelOptions:{name:"cgroup.memory.usage"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",attrs:{percentage:!1,integer:!0,forcey:10}},{name:"container.memory.usage",title:"Total Container Memory Usage (Mb)",directive:"area-stacked-time-series",dataAttrName:"data",dataModelType:o,dataModelOptions:{name:"container.memory.usage"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",attrs:{percentage:!1,integer:!0}},{name:"cgroup.memory.headroom",title:"Per-Container Memory Headroom (Mb)",directive:"line-time-series",dataAttrName:"data",dataModelType:d,dataModelOptions:{name:"cgroup.memory.headroom"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",requireContainerFilter:!0,attrs:{forcey:1,percentage:!1,integer:!0,area:!0}},{name:"cgroup.blkio.all.io_serviced",title:"Container Disk IOPS",directive:"line-time-series",dataAttrName:"data",dataModelType:l,dataModelOptions:{name:"cgroup.blkio.all.io_serviced",metricDefinitions:{"{key} read":"cgroup.blkio.all.io_serviced.read","{key} write":"cgroup.blkio.all.io_serviced.write"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",attrs:{percentage:!1,integer:!0}},{name:"cgroup.blkio.all.io_service_bytes",title:"Container Disk Throughput (Bytes)",directive:"line-time-series",dataAttrName:"data",dataModelType:l,dataModelOptions:{name:"cgroup.blkio.all.io_service_bytes",metricDefinitions:{"{key} read":"cgroup.blkio.all.io_service_bytes.read","{key} write":"cgroup.blkio.all.io_service_bytes.write"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",attrs:{percentage:!1,integer:!0}},{name:"cgroup.blkio.all.throttle.io_serviced",title:"Container Disk IOPS (Throttled)",directive:"line-time-series",dataAttrName:"data",dataModelType:l,dataModelOptions:{name:"cgroup.blkio.all.throttle.io_serviced",metricDefinitions:{"{key} read":"cgroup.blkio.all.throttle.io_serviced.read","{key} write":"cgroup.blkio.all.throttle.io_serviced.write"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",attrs:{percentage:!1,integer:!0}},{name:"cgroup.blkio.all.throttle.io_service_bytes",title:"Container Disk Throughput (Throttled) (Bytes)",directive:"line-time-series",dataAttrName:"data",dataModelType:l,dataModelOptions:{name:"cgroup.blkio.all.throttle.io_service_bytes",metricDefinitions:{"{key} read":"cgroup.blkio.all.throttle.io_service_bytes.read","{key} write":"cgroup.blkio.all.throttle.io_service_bytes.write"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",attrs:{percentage:!1,integer:!0}},{name:"cgroup.cpusched",title:"Per-Container CPU Scheduler",directive:"line-time-series",dataAttrName:"data",dataModelType:c,dataModelOptions:{name:"cgroup.cpusched",metricDefinitions:{"{key} shares":"cgroup.cpusched.shares","{key} periods":"cgroup.cpusched.periods"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",attrs:{percentage:!1,integer:!0}},{name:"cgroup.cpuacct.headroom",title:"Per-Container CPU Headroom",directive:"line-time-series",dataAttrName:"data",dataModelType:n,dataModelOptions:{name:"cgroup.cpuacct.headroom"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",requireContainerFilter:!0,attrs:{forcey:1,percentage:!0,integer:!1,area:!0}},{name:"cgroup.cpusched.throttled_time",title:"Per-Container Throttled CPU",directive:"line-time-series",dataAttrName:"data",dataModelType:l,dataModelOptions:{name:"cgroup.cpusched.throttled_time",metricDefinitions:{"{key}":"cgroup.cpusched.throttled_time"}},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",attrs:{forcey:5,percentage:!1,integer:!0}},{name:"cgroup.memory.utilization",title:"Per-Container Memory Utilization",directive:"line-time-series",dataAttrName:"data",dataModelType:b,dataModelOptions:{name:"cgroup.memory.utilization"},size:{width:"50%",height:"250px"},enableVerticalResize:!1,group:"Container",requireContainerFilter:!0,attrs:{forcey:1,percentage:!0,integer:!1,area:!1}}),C}e.$inject=["MetricDataModel","ConvertedMetricDataModel","CumulativeMetricDataModel","CgroupCPUUsageMetricDataModel","CgroupCPUHeadroomMetricDataModel","CgroupMemoryUsageMetricTimeSeriesDataModel","ContainerMemoryUsageMetricDataModel","ContainerNetworkBytesMetricDataModel","ContainerMultipleMetricDataModel","ContainerMultipleCumulativeMetricDataModel","CgroupMemoryHeadroomMetricDataModel","MemoryUtilizationMetricDataModel","NetworkBytesMetricDataModel","CpuUtilizationMetricDataModel","PerCpuUtilizationMetricDataModel","MultipleMetricDataModel","MultipleCumulativeMetricDataModel","DummyMetricDataModel","DiskLatencyMetricDataModel","CumulativeUtilizationMetricDataModel","CgroupMemoryUtilizationMetricDataModel","config"];var t=[{name:"kernel.all.cpu",size:{width:"50%"}},{name:"kernel.percpu.cpu",size:{width:"50%"}},{name:"kernel.all.runnable",size:{width:"50%"}},{name:"kernel.all.load",size:{width:"50%"}},{name:"network.interface.bytes",size:{width:"50%"}},{name:"network.tcpconn",size:{width:"50%"}},{name:"network.interface.packets",size:{width:"50%"}},{name:"network.tcp.retrans",size:{width:"50%"}},{name:"mem",size:{width:"50%"}},{name:"mem.vmstat.pgfault",size:{width:"50%"}},{name:"kernel.all.pswitch",size:{width:"50%"}},{name:"disk.iops",size:{width:"50%"}},{name:"disk.bytes",size:{width:"50%"}},{name:"disk.dev.avactive",size:{width:"50%"}},{name:"disk.dev.latency",size:{width:"50%"}}],i=[],a=[{name:"cgroup.cpuacct.usage",size:{width:"50%"}},{name:"container.memory.usage",size:{width:"50%"}},{name:"cgroup.memory.usage",size:{width:"50%"}},{name:"cgroup.memory.headroom",size:{width:"50%"}},{name:"container.disk.iops",size:{width:"50%"}},{name:"container.disk.bytes",size:{width:"50%"}}];angular.module("widget",["datamodel","chart","flamegraph","customWidgetSettings"]).factory("widgetDefinitions",e).value("defaultWidgets",t).value("emptyWidgets",i).value("containerWidgets",a)}(),/**!
 *
 *  Copyright 2016 Netflix, Inc.
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
function(){"use strict";function e(e){function t(t,n){var r={},o={};return t.backdrop="static",angular.extend(r,i,t),angular.extend(o,a,n),r.controller=["$scope","$uibModalInstance",function(e,t){e.modalOptions=o,e.modalOptions.ok=function(e){t.close(e)},e.modalOptions.close=function(){t.dismiss("cancel")}}],e.open(r).result}var i={backdrop:!0,keyboard:!0,modalFade:!0,template:'<div class="modal-header"><h3>{{modalOptions.headerText}}</h3></div><div class="modal-body"><p>{{modalOptions.bodyText}}</p></div><div class="modal-footer"><button type="button" class="btn" data-ng-click="modalOptions.close()">{{modalOptions.closeButtonText}}</button><button class="btn btn-primary" data-ng-click="modalOptions.ok();">{{modalOptions.actionButtonText}}</button></div>'},a={closeButtonText:"Close",actionButtonText:"OK",headerText:"Proceed?",bodyText:"Perform this action?"};return{showModal:t}}e.$inject=["$uibModal"],angular.module("modal",[]).factory("ModalService",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){function n(t){var n=i.properties.protocol+"://"+i.properties.host+":"+i.properties.port,r={};return r.method="GET",r.url=n+"/pmapi/context",r.params={},r.params[t.contextType]=t.contextValue,r.params.polltimeout=t.pollTimeout.toString(),r.timeout=5e3,e(r).then(function(e){return e.data.context?e.data.context:a.reject("context is undefined")})}function r(e,t){var i={};return i.contextType="hostspec",i.contextValue=e,i.pollTimeout=t,n(i)}function o(e,t){var i={};return i.contextType="hostname",i.contextValue=e,i.pollTimeout=t,n(i)}function s(e){var t={};return t.contextType="local",t.contextValue="ANYTHING",t.pollTimeout=e,n(t)}function c(e,t){var i={};return i.contextType="archivefile",i.contextValue=e,i.pollTimeout=t,n(i)}function l(t,n,r){var o=i.properties.protocol+"://"+i.properties.host+":"+i.properties.port,s={};return s.method="GET",s.url=o+"/pmapi/"+t+"/_fetch",s.params={},angular.isDefined(n)&&null!==n&&(s.params.names=n.join(",")),angular.isDefined(r)&&null!==r&&(s.params.pmids=r.join(",")),e(s).then(function(e){return angular.isUndefined(e.data.timestamp)||angular.isUndefined(e.data.timestamp.s)||angular.isUndefined(e.data.timestamp.us)||angular.isUndefined(e.data.values)?a.reject("metric values is empty"):e})}function d(t,n,r,o){var s=i.properties.protocol+"://"+i.properties.host+":"+i.properties.port,c={};return c.method="GET",c.url=s+"/pmapi/"+t+"/_indom",c.params={indom:n},angular.isDefined(r)&&null!==r&&(c.params.instance=r.join(",")),angular.isDefined(o)&&null!==o&&(c.params.inames=o.join(",")),c.cache=!0,e(c).then(function(e){return angular.isDefined(e.data.indom)||angular.isDefined(e.data.instances)?e:a.reject("instances is undefined")})}function u(t,n,r,o){var s=i.properties.protocol+"://"+i.properties.host+":"+i.properties.port,c={};return c.method="GET",c.url=s+"/pmapi/"+t+"/_indom",c.params={name:n},angular.isDefined(r)&&null!==r&&(c.params.instance=r.join(",")),angular.isDefined(o)&&null!==o&&(c.params.inames=o.join(",")),c.cache=!0,e(c).then(function(e){return angular.isDefined(e.data.instances)?e:a.reject("instances is undefined")})}function p(e){var t=1e3*e.data.timestamp.s+e.data.timestamp.us/1e3,i=e.data.values;return{timestamp:t,values:i}}function m(e){var t={};return angular.forEach(e,function(e){var i=e.data.indom,a=e.config.params.name,n={};angular.forEach(e.data.instances,function(e){n[e.instance.toString()]=e.name}),t[a.toString()]={indom:i,name:a,inames:n}}),t}function g(e,t){var i=a.defer(),n=[];return angular.forEach(t.values,function(t){var i=_.map(t.instances,function(e){return angular.isDefined(e.instance)&&null!==e.instance?e.instance:-1});n.push(u(e,t.name,i))}),a.all(n).then(function(e){var a=m(e),n={timestamp:t.timestamp,values:t.values,inames:a};i.resolve(n)},function(e){i.reject(e)},function(e){i.update(e)}),i.promise}function h(e,t){return l(e,t).then(p).then(function(t){return g(e,t)})}return{getHostspecContext:r,getHostnameContext:o,getLocalContext:s,getArchiveContext:c,getMetricsValues:l,getMetrics:h,getInstanceDomainsByIndom:d,getInstanceDomainsByName:u}}angular.module("pmapi",[]).factory("PMAPIService",e),e.$inject=["$http","$log","$rootScope","$q"]}(),/**!
 *
 *  Copyright 2016 Netflix, Inc.
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
function(){"use strict";function e(e,t,i,a,n,r,o,s,c,l){function d(e){var t=_.find(k,function(t){return t.name===e});return angular.isUndefined(t)?(t=new r(e),k.push(t)):t.subscribers++,t}function u(e){var t=_.find(k,function(t){return t.name===e});return angular.isUndefined(t)?(t=new o(e),k.push(t)):t.subscribers++,t}function p(e,t){var i=_.find(k,function(t){return t.name===e});return angular.isUndefined(i)?(i=new s(e,t),k.push(i)):i.subscribers++,i}function m(e,t){var i=_.find(k,function(t){return t.name===e});return angular.isUndefined(i)?(i=new c(e,t),k.push(i)):i.subscribers++,i}function g(e,t){var i=_.find(w,function(t){return t.name===e});return angular.isUndefined(i)?(i=new l(e,t),w.push(i)):i.subscribers++,i}function h(e){var t,i=_.find(k,function(t){return t.name===e});i.subscribers--,i.subscribers<1&&(t=k.indexOf(i),t>-1&&k.splice(t,1))}function f(e){var t,i=_.find(w,function(t){return t.name===e});i.subscribers--,i.subscribers<1&&(t=w.indexOf(i),t>-1&&w.splice(t,1))}function v(){angular.forEach(k,function(e){e.clearData()})}function y(){angular.forEach(w,function(e){e.clearData()})}function M(){return w}function b(){return k}var k=[],w=[];return{getOrCreateMetric:d,getOrCreateCumulativeMetric:u,getOrCreateConvertedMetric:p,getOrCreateCumulativeConvertedMetric:m,getOrCreateDerivedMetric:g,destroyMetric:h,destroyDerivedMetric:f,clearMetricList:v,clearDerivedMetricList:y,getSimpleMetricList:b,getDerivedMetricList:M}}e.$inject=["$rootScope","$http","$log","$q","PMAPIService","SimpleMetric","CumulativeMetric","ConvertedMetric","CumulativeConvertedMetric","DerivedMetric"],angular.module("metriclist",["pmapi","metric"]).factory("MetricListService",e)}(),/**!
 *
 *  Copyright 2016 Netflix, Inc.
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
function(){"use strict";angular.module("metric",["pmapi"])}(),/**!
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
function(){"use strict";function e(e){var t=function(e){this.name=e||null,this.data=[],this.subscribers=1};return t.prototype.toString=function(){return this.name},t.prototype.pushValue=function(t,i,a,n){var r,o,s=this;r=_.find(s.data,function(e){return e.iid===i}),angular.isDefined(r)&&null!==r?(r.values.push({x:t,y:n}),o=r.values.length-60*parseInt(e.properties.window)/parseInt(e.properties.interval),o>0&&r.values.splice(0,o)):(r={key:angular.isDefined(a)?a:this.name,iid:i,values:[{x:t,y:n},{x:t+1,y:n}]},s.data.push(r))},t.prototype.clearData=function(){this.data.length=0},t.prototype.deleteInvalidInstances=function(e){var t,i,a,n=this;angular.forEach(n.data,function(r){i=_.find(e,function(e){return t=angular.isUndefined(e.instance)?1:e.instance,t===r.iid}),angular.isUndefined(i)&&(a=n.data.indexOf(r),a>-1&&n.data.splice(a,1))})},t}e.$inject=["$rootScope"],angular.module("metric").factory("SimpleMetric",e)}(),/**!
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
function(){"use strict";function e(e,t,i){return{getInames:function(e,a){return i.getInstanceDomainsByName(t.properties.context,e,[a])}}}e.$inject=["$http","$rootScope","PMAPIService"],angular.module("metric").factory("MetricService",e)}(),/**!
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
function(){"use strict";function e(e){var t=function(e,t){this.name=e,this.data=[],this.subscribers=1,this.derivedFunction=t};return t.prototype.updateValues=function(){var t,i=this;t=i.derivedFunction(),t.length!==i.data.length&&(i.data.length=0),angular.forEach(t,function(t){var a,n=_.find(i.data,function(e){return e.key===t.key});angular.isUndefined(n)?(n={key:t.key,values:[{x:t.timestamp,y:t.value},{x:t.timestamp+1,y:t.value}]},i.data.push(n)):(n.values.push({x:t.timestamp,y:t.value}),a=n.values.length-60*parseInt(e.properties.window)/parseInt(e.properties.interval),a>0&&n.values.splice(0,a))})},t.prototype.clearData=function(){this.data.length=0},t}e.$inject=["$rootScope"],angular.module("metric").factory("DerivedMetric",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(e,t){this.base=a,this.base(e),this.conversionFunction=t};return a.prototype=new i,a.prototype.pushValue=function(t,i,a,n){var r,o,s,c,l=this;r=_.find(l.data,function(e){return e.iid===i}),angular.isUndefined(r)?(r={key:angular.isDefined(a)?a:this.name,iid:i,values:[],previousValue:n,previousTimestamp:t},l.data.push(r)):(s=(n-r.previousValue)/(t-r.previousTimestamp),c=l.conversionFunction(s),r.values.push({x:t,y:c}),r.previousValue=n,r.previousTimestamp=t,o=r.values.length-60*parseInt(e.properties.window)/parseInt(e.properties.interval),o>0&&r.values.splice(0,o))},a}e.$inject=["$rootScope","$log","SimpleMetric"],angular.module("metric").factory("CumulativeConvertedMetric",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(e){this.base=i,this.base(e)};return a.prototype=new i,a.prototype.pushValue=function(t,i,a,n){var r,o,s,c=this;r=_.find(c.data,function(e){return e.iid===i}),angular.isUndefined(r)?(r={key:angular.isDefined(a)?a:this.name,iid:i,values:[],previousValue:n,previousTimestamp:t},c.data.push(r)):(s=(n-r.previousValue)/((t-r.previousTimestamp)/1e3),r.values.length<1?r.values.push({x:t,y:s},{x:t+1,y:s}):r.values.push({x:t,y:s}),r.previousValue=n,r.previousTimestamp=t,o=r.values.length-60*parseInt(e.properties.window)/parseInt(e.properties.interval),o>0&&r.values.splice(0,o))},a}e.$inject=["$rootScope","$log","SimpleMetric"],angular.module("metric").factory("CumulativeMetric",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(e,t){this.base=i,this.base(e),this.conversionFunction=t};return a.prototype=new i,a.prototype.pushValue=function(t,i,a,n){var r,o,s,c=this;s=c.conversionFunction(n),r=_.find(c.data,function(e){return e.iid===i}),angular.isDefined(r)&&null!==r?(r.values.push({x:t,y:s}),o=r.values.length-60*parseInt(e.properties.window)/parseInt(e.properties.interval),o>0&&r.values.splice(0,o)):(r={key:angular.isDefined(a)?a:this.name,iid:i,values:[{x:t,y:s},{x:t+1,y:s}]},c.data.push(r))},a}e.$inject=["$rootScope","$log","SimpleMetric"],angular.module("metric").factory("ConvertedMetric",e)}(),/**!
 *
 *  Copyright 2016 Netflix, Inc.
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
function(){"use strict";angular.module("flamegraph",["dashboard"])}(),/**!
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
function(){"use strict";function e(e,t,i,a){function n(){i.get(t.properties.protocol+"://"+t.properties.host+":"+t.properties.port+"/pmapi/"+t.properties.context+"/_fetch?names=generic.systack").success(function(){a.success("generic.systack requested.","Success")}).error(function(){a.error("Failed requesting generic.systack.","Error")})}return{generate:n}}e.$inject=["$log","$rootScope","$http","toastr"],angular.module("flamegraph").factory("FlameGraphService",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){function n(n){n.host=e.properties.host,n.port=e.properties.port,n.context=e.properties.context,n.ready=!1,n.processing=!1,n.id=a.getGuid(),n.generateFlameGraph=function(){i.generate(),n.ready=!0,n.processing=!0,t(function(){n.processing=!1},65e3)}}return{restrict:"A",templateUrl:"app/components/flamegraph/flamegraph.html",link:n}}e.$inject=["$rootScope","$timeout","FlameGraphService","DashboardService"],angular.module("flamegraph").directive("cpuFlameGraph",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a,n,r,o,s,c,l){function d(){k&&i.cancel(k)}function u(t){t?w=0:(r.error("Failed fetching metrics. Trying again.","Error"),w+=1),w>5&&(d(k),w=0,e.properties.context="-1",e.flags.contextAvailable=!1,r.error("Consistently failed fetching metrics from host (>5). Please update the hostname to resume operation.","Error"))}function p(t){var i=[],a=e.properties.context,n=c.getSimpleMetricList();a&&a>0&&n.length>0&&(angular.forEach(n,function(e){i.push(e.name)}),s.getMetrics(a,i).then(function(e){var t,i,a,r;if(e.values.length!==n.length){var o;angular.forEach(n,function(t){o=_.find(e.values,function(e){return e.name===t.name}),angular.isUndefined(o)&&t.clearData()})}angular.forEach(e.values,function(o){t=o.name,i=_.find(n,function(e){return e.name===t}),o.instances.length!==i.data.length&&i.deleteInvalidInstances(o.instances),angular.isDefined(i)&&null!==i&&angular.forEach(o.instances,function(n){a=angular.isUndefined(n.instance)?1:n.instance,r=e.inames[t].inames[a],i.pushValue(e.timestamp,a,r,n.value)})})}).then(function(){t(!0),e.$broadcast("updateMetrics")},function(e){400===e.status&&-1!==e.data.indexOf("-12376")&&v(),t(!1)}))}function m(){var t=c.getDerivedMetricList();t.length>0&&(angular.forEach(t,function(e){e.updateValues()}),e.$broadcast("updateDerivedMetrics"))}function g(){p(u),m()}function h(){d(k),e.properties.host&&(e.properties.context&&e.properties.context>0?k=i(g,1e3*parseInt(e.properties.interval)):r.error("Vector is not connected to the host. Please update the hostname to resume operation.","Error"))}function f(t){var i=null;t&&(i=t.match("(.*):([0-9]*)"),null!==i?(e.properties.host=i[1],e.properties.port=i[2]):e.properties.host=t)}function v(){e.flags.contextUpdating=!0,e.flags.contextAvailable=!1,s.getHostspecContext(e.properties.hostspec,600).then(function(t){e.flags.contextUpdating=!1,e.flags.contextAvailable=!0,e.properties.context=t,h(),s.getMetrics(t,["pmcd.hostname"]).then(function(t){e.properties.hostname=t.values[0].instances[0].value},function(){e.properties.hostname="Hostname not available.",a.error("Error fetching hostname.")})},function(){r.error("Failed fetching context from host. Try updating the hostname.","Error"),e.flags.contextUpdating=!1,e.flags.contextAvailable=!1})}function y(t){n.search("host",t),n.search("hostspec",e.properties.hostspec),e.properties.context=-1,e.properties.hostname=null,e.properties.port=o.port,c.clearMetricList(),c.clearDerivedMetricList(),f(t),v()}function M(){e.properties?(e.properties.interval||(e.properties.interval=o.interval),e.properties.window||(e.properties.window=o.window),e.properties.protocol||(e.properties.protocol=o.protocol),e.properties.host||(e.properties.host=""),e.properties.hostspec||(e.properties.hostspec=o.hostspec),e.properties.port||(e.properties.port=o.port),!e.properties.context||e.properties.context<0?v():h()):e.properties={protocol:o.protocol,host:"",hostspec:o.hostspec,port:o.port,context:-1,hostname:null,window:o.window,interval:o.interval,containerFilter:"",containerList:[],selectedContainer:""},e.flags={contextAvailable:!1,contextUpdating:!1,isHostnameExpanded:o.expandHostname,enableContainerWidgets:o.enableContainerWidgets,disableHostspecInput:o.disableHostspecInput,disableContainerFilter:o.disableContainerFilter,disableContainerSelect:o.disableContainerSelect,containerSelectOverride:o.containerSelectOverride,disableContainerSelectNone:!1,disableHostnameInputContainerSelect:o.disableHostnameInputContainerSelect},o.enableContainerWidgets&&l.initialize()}function b(){return Math.floor(65536*(1+Math.random())).toString(16).substring(1)}var k,w=0;return{cancelInterval:d,updateInterval:h,updateHost:y,updateContext:v,getGuid:b,initialize:M}}e.$inject=["$rootScope","$http","$interval","$log","$location","toastr","config","PMAPIService","MetricListService","ContainerMetadataService"],angular.module("dashboard",["pmapi","metriclist","containermetadata"]).factory("DashboardService",e)}(),/**!
 *
 *  Copyright 2016 Netflix, Inc.
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
function(){"use strict";angular.module("datamodel",["containermetadata","dashboard","metriclist"])}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metric=t.getOrCreateMetric(this.name),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyMetric(this.name),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("MetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=t.getOrCreateCumulativeMetric("kernel.percpu.cpu.sys"),r=t.getOrCreateCumulativeMetric("kernel.percpu.cpu.user");a=function(){var e,t,i,a=[];return angular.forEach(n.data,function(n){n.values.length>0&&(e=_.find(r.data,function(e){return e.key===n.key}),angular.isDefined(e)&&(t=n.values[n.values.length-1],i=e.values[e.values.length-1],t.x===i.x&&a.push({timestamp:t.x,key:n.key,value:(t.y+i.y)/1e3})))}),a},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("kernel.percpu.cpu.sys"),t.destroyMetric("kernel.percpu.cpu.user"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("PerCpuUtilizationMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=this,r=t.getOrCreateCumulativeMetric("network.interface.in.bytes"),o=t.getOrCreateCumulativeMetric("network.interface.out.bytes");a=function(){var e,t=[],i=function(i,a){i.values.length>0&&-1!==i.key.indexOf(n.widgetScope.widget.filter)&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:i.key+a,value:e.y/1024}))};return angular.forEach(r.data,function(e){i(e," in")}),angular.forEach(o.data,function(e){i(e," out")}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("network.interface.in.bytes"),t.destroyMetric("network.interface.out.bytes"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("NetworkBytesMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metricDefinitions=this.dataModelOptions.metricDefinitions;var a,n={};angular.forEach(this.metricDefinitions,function(e,i){n[i]=t.getOrCreateMetric(e)}),a=function(){var e,t=[];return angular.forEach(n,function(i,a){angular.forEach(i.data,function(i){i.values.length>0&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:a.replace("{key}",i.key),value:e.y}))})}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),angular.forEach(this.metricDefinitions,function(e){t.destroyMetric(e)}),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("MultipleMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metricDefinitions=this.dataModelOptions.metricDefinitions;var a,n=this,r={};angular.forEach(this.metricDefinitions,function(e,i){r[i]=t.getOrCreateCumulativeMetric(e)}),a=function(){var e,t=[];return angular.forEach(r,function(i,a){angular.forEach(i.data,function(i){i.values.length>0&&(angular.isUndefined(n.widgetScope.widget.filter)||-1!==i.key.indexOf(n.widgetScope.widget.filter))&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:a.replace("{key}",i.key),value:e.y}))})}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),angular.forEach(this.metricDefinitions,function(e){t.destroyMetric(e)}),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("MultipleCumulativeMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=function(e){return e/1024},r=t.getOrCreateConvertedMetric("mem.util.cached",n),o=t.getOrCreateConvertedMetric("mem.util.used",n),s=t.getOrCreateConvertedMetric("mem.util.free",n),c=t.getOrCreateConvertedMetric("mem.util.bufmem",n);a=function(){var e,t,i,a,n=[];return e=function(){if(o.data.length>0){var e=o.data[o.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),t=function(){if(r.data.length>0){var e=r.data[r.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),i=function(){if(s.data.length>0){var e=s.data[s.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),a=function(){if(c.data.length>0){var e=c.data[c.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),angular.isDefined(e)&&angular.isDefined(t)&&angular.isDefined(a)&&n.push({timestamp:e.x,key:"application",value:e.y-t.y-a.y}),angular.isDefined(t)&&angular.isDefined(a)&&n.push({timestamp:e.x,key:"free (cache)",value:t.y+a.y}),angular.isDefined(i)&&n.push({timestamp:e.x,key:"free (unused)",value:i.y}),n},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("mem.util.cached"),t.destroyMetric("mem.util.used"),t.destroyMetric("mem.util.free"),t.destroyMetric("mem.util.bufmem"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("MemoryUtilizationMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t){var i=function(){return this};return i.prototype=Object.create(e.prototype),i.prototype.init=function(){e.prototype.init.call(this),this.metric=t.getOrCreateMetric("kernel.uname.release")},i.prototype.destroy=function(){t.destroyMetric("kernel.uname.release"),e.prototype.destroy.call(this)},i}e.$inject=["WidgetDataModel","MetricListService"],angular.module("datamodel").factory("DummyMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=t.getOrCreateCumulativeMetric("disk.dev.read_rawactive"),r=t.getOrCreateCumulativeMetric("disk.dev.write_rawactive"),o=t.getOrCreateCumulativeMetric("disk.dev.read"),s=t.getOrCreateCumulativeMetric("disk.dev.write");a=function(){function e(e,n,r,o){e.data.length>0&&angular.forEach(e.data,function(e){t=_.find(n.data,function(t){return t.key===e.key}),angular.isDefined(t)&&e.values.length>0&&t.values.length>0&&(i=e.values[e.values.length-1],a=t.values[e.values.length-1],c=i.y>0?a.y/i.y:0,o.push({timestamp:i.x,key:e.key+r,value:c}))})}var t,i,a,c,l=[];return e(o,n," read latency",l),e(s,r," write latency",l),l},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("disk.dev.read_rawactive"),t.destroyMetric("disk.dev.write_rawactive"),t.destroyMetric("disk.dev.read"),t.destroyMetric("disk.dev.write"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("DiskLatencyMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=t.getOrCreateCumulativeMetric(this.name);a=function(){var e,t=[];return angular.forEach(n.data,function(i){i.values.length>0&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:i.key,value:e.y/1e3}))}),t},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric(this.name),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("CumulativeUtilizationMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metric=t.getOrCreateCumulativeMetric(this.name),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyMetric(this.name),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("CumulativeMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=t.getOrCreateCumulativeMetric("kernel.all.cpu.sys"),r=t.getOrCreateCumulativeMetric("kernel.all.cpu.user"),o=t.getOrCreateMetric("hinv.ncpu");a=function(){var e,t,i=[];if(o.data.length>0&&(e=o.data[o.data.length-1],e.values.length>0)){t=e.values[e.values.length-1].y;var a=function(e,a){if(e.values.length>0){var n=e.values[e.values.length-1];i.push({timestamp:n.x,key:a,value:n.y/(1e3*t)})}};angular.forEach(n.data,function(e){a(e,"sys")}),angular.forEach(r.data,function(e){a(e,"user")})}return i},this.metric=t.getOrCreateDerivedMetric(this.name,a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("kernel.all.cpu.sys"),t.destroyMetric("kernel.all.cpu.user"),t.destroyMetric("hinv.ncpu"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("CpuUtilizationMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.conversionFunction=this.dataModelOptions.conversionFunction,this.metric=t.getOrCreateConvertedMetric(this.name,this.conversionFunction),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyMetric(this.name),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("ConvertedMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i){var a=function(){return this};return a.prototype=Object.create(e.prototype),a.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var a,n=this,r=t.getOrCreateCumulativeMetric("network.interface.in.bytes"),o=t.getOrCreateCumulativeMetric("network.interface.out.bytes");a=function(){var e,t=[];return angular.forEach(r.data,function(i){i.values.length>0&&-1!==i.key.indexOf("veth")&&-1!==i.key.indexOf(n.widgetScope.widget.filter)&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:i.key+" in",value:e.y/1024}))}),angular.forEach(o.data,function(i){i.values.length>0&&-1!==i.key.indexOf("veth")&&-1!==i.key.indexOf(n.widgetScope.widget.filter)&&(e=i.values[i.values.length-1],t.push({timestamp:e.x,key:i.key+" out",value:e.y/1024}))}),t},this.metric=t.getOrCreateDerivedMetric("container.network.interface",a),this.updateScope(this.metric.data)},a.prototype.destroy=function(){t.destroyDerivedMetric("container.network.interface"),t.destroyMetric("network.interface.in.bytes"),t.destroyMetric("network.interface.out.bytes"),e.prototype.destroy.call(this)},a}e.$inject=["WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("ContainerNetworkBytesMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){var n=function(){return this};return n.prototype=Object.create(e.prototype),n.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metricDefinitions=this.dataModelOptions.metricDefinitions;var n,r={};angular.forEach(this.metricDefinitions,function(e,i){r[i]=t.getOrCreateMetric(e)}),n=function(){var e,t,i=[];return angular.forEach(r,function(n,r){angular.forEach(n.data,function(n){a.containerIdExist(n.key)&&(t=a.idDictionary(n.key)||n.key,n.values.length>0&&a.checkContainerName(t)&&a.checkContainerFilter(t)&&(e=n.values[n.values.length-1],i.push({timestamp:e.x,key:r.replace("{key}",t),value:e.y})))})}),i},this.metric=t.getOrCreateDerivedMetric(this.name,n),this.updateScope(this.metric.data)},n.prototype.destroy=function(){t.destroyDerivedMetric(this.name),angular.forEach(this.metricDefinitions,function(e){t.destroyMetric(e)}),e.prototype.destroy.call(this)},n}e.$inject=["WidgetDataModel","MetricListService","DashboardService","ContainerMetadataService"],angular.module("datamodel").factory("ContainerMultipleMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){var n=function(){return this};return n.prototype=Object.create(e.prototype),n.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid(),this.metricDefinitions=this.dataModelOptions.metricDefinitions;var n,r={};angular.forEach(this.metricDefinitions,function(e,i){r[i]=t.getOrCreateCumulativeMetric(e)}),n=function(){var e,t,i=[];return angular.forEach(r,function(n,r){angular.forEach(n.data,function(n){a.containerIdExist(n.key)&&(t=a.idDictionary(n.key)||n.key,n.values.length>0&&a.checkContainerName(t)&&a.checkContainerFilter(t)&&(e=n.values[n.values.length-1],i.push({timestamp:e.x,key:r.replace("{key}",t),value:e.y})))})}),i},this.metric=t.getOrCreateDerivedMetric(this.name,n),this.updateScope(this.metric.data)},n.prototype.destroy=function(){t.destroyDerivedMetric(this.name),angular.forEach(this.metricDefinitions,function(e){t.destroyMetric(e)}),e.prototype.destroy.call(this)},n}e.$inject=["WidgetDataModel","MetricListService","DashboardService","ContainerMetadataService"],angular.module("datamodel").factory("ContainerMultipleCumulativeMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){var n=function(){return this};return n.prototype=Object.create(e.prototype),n.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var n,r=function(e){return e/1024/1024},o=function(e){return e/1024},s=t.getOrCreateConvertedMetric("mem.util.used",o),c=t.getOrCreateConvertedMetric("mem.util.free",o),l=t.getOrCreateConvertedMetric("cgroup.memory.usage",r);n=function(){var e,t,i,n=[];return e=function(){if(s.data.length>0){var e=s.data[s.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),t=function(){if(c.data.length>0){var e=c.data[c.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),i=function(){if(l.data.length>0){var e=0,t=l.data[l.data.length-1].values[l.data[l.data.length-1].values.length-1].x;return angular.forEach(l.data,function(t){t.values.length>0&&a.containerIdExist(t.key)&&(e+=t.values[t.values.length-1].y)}),{x:t,y:e}}}(),angular.isDefined(e)&&angular.isDefined(i)&&n.push({timestamp:e.x,key:"host used",value:e.y-i.y}),angular.isDefined(t)&&n.push({timestamp:t.x,key:"free (unused)",value:t.y}),angular.isDefined(i)&&angular.isDefined(e)&&n.push({timestamp:i.x,key:"container used",value:i.y}),n},this.metric=t.getOrCreateDerivedMetric(this.name,n),this.updateScope(this.metric.data)},n.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("mem.util.used"),t.destroyMetric("mem.util.free"),t.destroyMetric("cgroup.memory.usage"),e.prototype.destroy.call(this)},n}e.$inject=["WidgetDataModel","MetricListService","DashboardService","ContainerMetadataService"],angular.module("datamodel").factory("ContainerMemoryUsageMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){var n=function(){return this};return n.prototype=Object.create(e.prototype),n.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var n,r=function(e){return e/1024/1024},o=function(e){return e/1024},s=t.getOrCreateConvertedMetric("cgroup.memory.usage",r),c=t.getOrCreateConvertedMetric("cgroup.memory.limit",r),l=t.getOrCreateConvertedMetric("mem.physmem",o);n=function(){var e,t=[];return e=function(){if(l.data.length>0){var e=l.data[l.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),angular.forEach(s.data,function(i){if(i.values.length>0&&a.containerIdExist(i.key)){var n=i.values[i.values.length-1],r=a.idDictionary(i.key)||i.key;if(a.checkContainerName(r)&&a.checkContainerFilter(r)){var o=_.find(c.data,function(e){return e.key===i.key});if(angular.isDefined(o)){var s=o.values[o.values.length-1];s.y>=e.y?t.push({timestamp:n.x,key:r,value:n.y/e.y}):t.push({timestamp:n.x,key:r,value:n.y/s.y})}}}}),t},this.metric=t.getOrCreateDerivedMetric(this.name,n),this.updateScope(this.metric.data)},n.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("cgroup.memory.usage"),t.destroyMetric("cgroup.memory.limit"),t.destroyMetric("mem.physmem"),e.prototype.destroy.call(this)},n}e.$inject=["WidgetDataModel","MetricListService","DashboardService","ContainerMetadataService"],angular.module("datamodel").factory("CgroupMemoryUtilizationMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){var n=function(){return this};return n.prototype=Object.create(t.prototype),n.prototype.init=function(){t.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+a.getGuid();var n,r=function(e){return e/1024/1024},o=i.getOrCreateConvertedMetric("cgroup.memory.usage",r);n=function(){var t,i,a=[];return angular.forEach(o.data,function(n){n.values.length>0&&e.containerIdExist(n.key)&&(t=n.values[n.values.length-1],i=e.idDictionary(n.key)||n.key,e.checkContainerName(i)&&e.checkContainerFilter(i)&&a.push({timestamp:t.x,key:i,value:t.y}))}),a},this.metric=i.getOrCreateDerivedMetric(this.name,n),this.updateScope(this.metric.data)},n.prototype.destroy=function(){i.destroyDerivedMetric(this.name),i.destroyMetric("cgroup.memory.usage"),t.prototype.destroy.call(this)},n}e.$inject=["ContainerMetadataService","WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("CgroupMemoryUsageMetricTimeSeriesDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){var n=function(){return this};return n.prototype=Object.create(e.prototype),n.prototype.init=function(){e.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+i.getGuid();var n,r=function(e){return e/1024/1024},o=function(e){return e/1024},s=t.getOrCreateConvertedMetric("cgroup.memory.usage",r),c=t.getOrCreateConvertedMetric("cgroup.memory.limit",r),l=t.getOrCreateConvertedMetric("mem.physmem",o);n=function(){var e,t=[];return e=function(){if(l.data.length>0){var e=l.data[l.data.length-1];if(e.values.length>0)return e.values[e.values.length-1]}}(),angular.forEach(s.data,function(e){if(e.values.length>0&&a.containerIdExist(e.key)){var i=e.values[e.values.length-1],n=a.idDictionary(e.key)||e.key;a.checkContainerName(n)&&a.checkContainerFilter(n)&&t.push({timestamp:i.x,key:n+" used",value:i.y})}}),angular.forEach(c.data,function(i){if(i.values.length>0&&a.containerIdExist(i.key)){var n=i.values[i.values.length-1],r=a.idDictionary(i.key)||i.key;a.checkContainerName(r)&&a.checkContainerFilter(r)&&(n.y>=e.y?t.push({timestamp:e.x,key:r+" limit (physical)",value:e.y}):t.push({timestamp:n.x,key:r+" limit",value:n.y}))}}),t},this.metric=t.getOrCreateDerivedMetric(this.name,n),this.updateScope(this.metric.data)},n.prototype.destroy=function(){t.destroyDerivedMetric(this.name),t.destroyMetric("cgroup.memory.usage"),t.destroyMetric("cgroup.memory.limit"),t.destroyMetric("mem.physmem"),e.prototype.destroy.call(this)},n}e.$inject=["WidgetDataModel","MetricListService","DashboardService","ContainerMetadataService"],angular.module("datamodel").factory("CgroupMemoryHeadroomMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){var n=function(){return this};return n.prototype=Object.create(t.prototype),n.prototype.init=function(){t.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+a.getGuid();var n,r=i.getOrCreateCumulativeMetric("cgroup.cpuacct.usage");n=function(){var t,i,a=[];return r.data.length>0&&angular.forEach(r.data,function(n){n.values.length>0&&e.containerIdExist(n.key)&&(t=n.values[n.values.length-1],i=e.idDictionary(n.key)||n.key,e.checkContainerName(i)&&e.checkContainerFilter(i)&&a.push({timestamp:t.x,key:i,value:t.y/1e3/1e3/1e3}))}),a},this.metric=i.getOrCreateDerivedMetric(this.name,n),this.updateScope(this.metric.data)},n.prototype.destroy=function(){i.destroyDerivedMetric(this.name),i.destroyMetric("cgroup.cpuacct.usage"),t.prototype.destroy.call(this)},n}e.$inject=["ContainerMetadataService","WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("CgroupCPUUsageMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a){var n=function(){return this};return n.prototype=Object.create(t.prototype),n.prototype.init=function(){t.prototype.init.call(this),this.name=this.dataModelOptions?this.dataModelOptions.name:"metric_"+a.getGuid();var n,r=i.getOrCreateCumulativeMetric("cgroup.cpuacct.usage"),o=i.getOrCreateMetric("cgroup.cpusched.shares"),s=i.getOrCreateMetric("cgroup.cpusched.periods"),c=i.getOrCreateMetric("hinv.ncpu");n=function(){var t=[];return r.data.length>0&&angular.forEach(r.data,function(i){if(i.values.length>0&&e.containerIdExist(i.key)){var a=i.values[i.values.length-1],n=e.idDictionary(i.key)||i.key;e.checkContainerName(n)&&e.checkContainerFilter(n)&&t.push({timestamp:a.x,key:n,value:a.y/1e3/1e3/1e3})}}),s.data.length>0&&angular.forEach(s.data,function(i){if(i.values.length>0&&e.containerIdExist(i.key)){var a=i.values[i.values.length-1],n=e.idDictionary(i.key)||i.key;if(e.checkContainerName(n)&&e.checkContainerFilter(n))if(a.y>0){var r=_.find(o.data,function(e){return e.key===i.key});if(angular.isDefined(r)){var s=r.values[r.values.length-1];t.push({timestamp:a.x,key:n+" (limit)",value:s.y/a.y})}}else if(c.data.length>0){var l=c.data[c.data.length-1];if(l.values.length>0){var d=l.values[l.values.length-1];t.push({timestamp:d.x,key:n+" (physical)",value:d.y})}}}}),t},this.metric=i.getOrCreateDerivedMetric(this.name,n),this.updateScope(this.metric.data)},n.prototype.destroy=function(){i.destroyDerivedMetric(this.name),i.destroyMetric("cgroup.cpuacct.usage"),t.prototype.destroy.call(this)},n}e.$inject=["ContainerMetadataService","WidgetDataModel","MetricListService","DashboardService"],angular.module("datamodel").factory("CgroupCPUHeadroomMetricDataModel",e)}(),/**!
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
function(){"use strict";function e(){function e(){return function(e){return d3.time.format("%X")(new Date(e))}}function t(){return function(e){return d3.format(".02f")(e)}}function i(){return function(e){return d3.format("f")(e)}}function a(){return function(e){return d3.format("%")(e)}}function n(){return function(e){return e.x}}function r(){return function(e){return e.y}}function o(){return"chart_"+Math.floor(65536*(1+Math.random())).toString(16).substring(1)}return{xAxisTickFormat:e,yAxisTickFormat:t,yAxisIntegerTickFormat:i,yAxisPercentageTickFormat:a,xFunction:n,yFunction:r,getId:o}}angular.module("d3",[]).factory("D3Service",e)}(),/**!
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
function(){"use strict";function e(e,t,i){e.widget=i,e.result=angular.extend({},e.result,i),e.ok=function(){t.close(e.result)},e.cancel=function(){t.dismiss("cancel")}}e.$inject=["$scope","$uibModalInstance","widget"],angular.module("customWidgetSettings",[]).controller("CustomWidgetSettingsController",e)}(),/**!
 *
 *  Copyright 2016 Netflix, Inc.
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
function(){"use strict";function e(e,t,i,a,n,r,o,s,c,l){function d(e){return T[u(e)]}function u(e){return null===e?!1:(-1!==e.indexOf("docker/")?e=e.split("/")[2]:-1!==e.indexOf("/docker-")&&(e=e.split("-")[1].split(".")[0]),e)}function p(){T={}}function m(e){return angular.isDefined(T[u(e)])&&""!==T[u(e)]}function g(){v(),t.properties.containerList=y(),D?-1===t.properties.containerList.indexOf(n.container)&&(t.properties.selectedContainer=""):angular.isDefined(n.container)?-1!==t.properties.containerList.indexOf(n.container)&&(t.properties.selectedContainer=n.container,t.flags.disableContainerSelectNone=!0,D=!0):D=!0}function h(){O=l.getOrCreateMetric("containers.cgroup"),S=l.getOrCreateMetric("containers.name"),z=t.$on("updateMetrics",g)}function f(e){var t;return"undefined"==typeof C?!c.useCgroupId&&(t=_.find(S.data,function(t){return t.key===e}))?t.values[t.values.length-1].y:e.substring(0,12):(t=_.find(S.data,function(t){return t.key===e}),C.resolve(e,t))}function v(){T=O.data.reduce(function(e,t){return e[t.key]=f(t.key),e},{})}function y(){return O.data.reduce(function(e,t){var i=f(t.key);return angular.isDefined(i)&&e.push(i),e},[])}function M(e){return""===t.properties.containerFilter||-1!==e.indexOf(t.properties.containerFilter)}function b(e){return""===t.properties.selectedContainer||-1!==e.indexOf(t.properties.selectedContainer)}function k(){r.search("container",t.properties.selectedContainer),""!==t.properties.selectedContainer?t.flags.disableContainerSelectNone=!0:t.flags.disableContainerSelectNone=!1}function w(){r.search("containerFilter",t.properties.containerFilter)}var C,D=!1;try{C=o.get("containerNameResolver")}catch(x){s.debug("No external container name resolver defined.")}var O,S,z,T={};return{idDictionary:d,getContainerList:y,updateIdDictionary:v,clearIdDictionary:p,checkContainerFilter:M,containerIdExist:m,checkContainerName:b,updateContainer:k,updateContainerFilter:w,initialize:h}}e.$inject=["$http","$rootScope","$q","$interval","$routeParams","$location","$injector","$log","config","MetricListService"],angular.module("containermetadata",["metriclist"]).factory("ContainerMetadataService",e)}(),/**!
 *
 *  Copyright 2016 Netflix, Inc.
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
nv.models.tooltip=function(){"use strict";function e(){if(!u){var e=document.body;u=d3.select(e).append("div").attr("class","nvtooltip "+(s?s:"xy-tooltip")).attr("id",i),u.style("top",0).style("left",0),u.style("opacity",0),u.style("position","fixed"),u.selectAll("div, table, td, tr").classed(f,!0),u.classed(f,!0)}}function t(){return m&&w(a)?(nv.dom.write(function(){e();var t=k(a);t&&(u.node().innerHTML=t),D()}),t):void 0}var i="nvtooltip-"+Math.floor(1e5*Math.random()),a=null,n="w",r=25,o=0,s=null,c=null,l=!0,d=200,u=null,p={left:null,top:null},m=!0,g=100,h=!0,f="nv-pointer-events-none",v=function(){return{left:d3.event.clientX,top:d3.event.clientY}},y=function(e){return e},M=function(e){return e},b=function(e){return e},k=function(e){if(null===e)return"";var t=d3.select(document.createElement("table"));if(h){var i=t.selectAll("thead").data([e]).enter().append("thead");i.append("tr").append("td").attr("colspan",3).append("strong").classed("x-value",!0).html(M(e.value))}var a=t.selectAll("tbody").data([e]).enter().append("tbody"),n=a.selectAll("tr").data(function(e){return e.series}).enter().append("tr").classed("highlight",function(e){return e.highlight});n.append("td").classed("legend-color-guide",!0).append("div").style("background-color",function(e){return e.color}),n.append("td").classed("key",!0).classed("total",function(e){return!!e.total}).html(function(e,t){return b(e.key,t)}),n.append("td").classed("value",!0).html(function(e,t){return y(e.value,t)}),n.selectAll("td").each(function(e){if(e.highlight){var t=d3.scale.linear().domain([0,1]).range(["#fff",e.color]),i=.6;d3.select(this).style("border-bottom-color",t(i)).style("border-top-color",t(i))}});var r=t.node().outerHTML;return void 0!==e.footer&&(r+='<div class="footer">'+e.footer+"</div>"),r},w=function(e){if(e&&e.series){if(e.series instanceof Array)return!!e.series.length;if(e.series instanceof Object)return e.series=[e.series],!0}return!1},C=function(e){var t,i,a,o=u.node().offsetHeight,s=u.node().offsetWidth,c=document.documentElement.clientWidth,l=document.documentElement.clientHeight;switch(n){case"e":t=-s-r,i=-(o/2),e.left+t<0&&(t=r),(a=e.top+i)<0&&(i-=a),(a=e.top+i+o)>l&&(i-=a-l);break;case"w":t=r,i=-(o/2),e.left+t+s>c&&(t=-s-r),(a=e.top+i)<0&&(i-=a),(a=e.top+i+o)>l&&(i-=a-l);break;case"n":t=-(s/2)-5,i=r,e.top+i+o>l&&(i=-o-r),(a=e.left+t)<0&&(t-=a),(a=e.left+t+s)>c&&(t-=a-c);break;case"s":t=-(s/2),i=-o-r,e.top+i<0&&(i=r),(a=e.left+t)<0&&(t-=a),(a=e.left+t+s)>c&&(t-=a-c);break;case"center":t=-(s/2),i=-(o/2);break;default:t=0,i=0}return{left:t,top:i}},D=function(){nv.dom.read(function(){var e=v(),t=C(e),i=e.left+t.left,a=e.top+t.top;if(l)u.interrupt().transition().delay(d).duration(0).style("opacity",0);else{var n="translate("+p.left+"px, "+p.top+"px)",r="translate("+i+"px, "+a+"px)",o=d3.interpolateString(n,r),s=u.style("opacity")<.1;u.interrupt().transition().duration(s?0:g).styleTween("transform",function(){return o},"important").styleTween("-webkit-transform",function(){return o}).style("-ms-transform",r).style("opacity",1)}p.left=i,p.top=a})};return t.nvPointerEventsClass=f,t.options=nv.utils.optionsFunc.bind(t),t._options=Object.create({},{duration:{get:function(){return g},set:function(e){g=e}},gravity:{get:function(){return n},set:function(e){n=e}},distance:{get:function(){return r},set:function(e){r=e}},snapDistance:{get:function(){return o},set:function(e){o=e}},classes:{get:function(){return s},set:function(e){s=e}},chartContainer:{get:function(){return c},set:function(e){c=e}},enabled:{get:function(){return m},set:function(e){m=e}},hideDelay:{get:function(){return d},set:function(e){d=e}},contentGenerator:{get:function(){return k},set:function(e){k=e}},valueFormatter:{get:function(){return y},set:function(e){y=e}},headerFormatter:{get:function(){return M},set:function(e){M=e}},keyFormatter:{get:function(){return b},set:function(e){b=e}},headerEnabled:{get:function(){return h},set:function(e){h=e}},position:{get:function(){return v},set:function(e){v=e}},hidden:{get:function(){return l},set:function(e){l!==e&&(l=!!e,t())}},data:{get:function(){return a},set:function(e){e.point&&(e.value=e.point.x,e.series=e.series||{},e.series.value=e.point.y,e.series.color=e.point.color||e.series.color),a=e}},node:{get:function(){return u.node()},set:function(){}},id:{get:function(){return i},set:function(){}}}),nv.utils.initOptions(t),t},/**!
 *
 *  Copyright 2016 Netflix, Inc.
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
function(){"use strict";angular.module("chart",["d3","dashboard"])}(),/**!
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
function(){"use strict";function e(e,t,i){function a(t){t.id=i.getId(),t.flags=e.flags;var a;nv.addGraph(function(){var e=250;return a=nv.models.lineChart().options({duration:0,useInteractiveGuideline:!0,interactive:!1,showLegend:!0,showXAxis:!0,showYAxis:!0}),a.margin({left:35,right:35}),a.height(e),t.forcey&&a.forceY([0,t.forcey]),a.x(i.xFunction()),a.y(i.yFunction()),a.xAxis.tickFormat(i.xAxisTickFormat()),t.percentage?a.yAxis.tickFormat(i.yAxisPercentageTickFormat()):t.integer?a.yAxis.tickFormat(i.yAxisIntegerTickFormat()):a.yAxis.tickFormat(i.yAxisTickFormat()),nv.utils.windowResize(a.update),d3.select("#"+t.id+" svg").datum(t.data).style("height",e+"px").transition().duration(0).call(a),a}),t.$on("updateMetrics",function(){t.area&&angular.forEach(t.data,function(e){e.area=!0}),a.update()})}return{restrict:"A",templateUrl:"app/components/chart/chart.html",scope:{data:"=",percentage:"=",integer:"=",forcey:"=",area:"="},link:a}}e.$inject=["$rootScope","$log","D3Service"],angular.module("chart").directive("lineTimeSeries",e)}(),/**!
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
function(){"use strict";function e(e,t,i){function a(t){t.id=i.getId(),t.flags=e.flags,t.legend=!0;var a;nv.addGraph(function(){var e=i.yAxisTickFormat(),n=250;return a=nv.models.stackedAreaChart().options({duration:0,useInteractiveGuideline:!0,interactive:!1,showLegend:!0,showXAxis:!0,showYAxis:!0,showControls:!1}),a.margin({left:35,right:35}),a.height(n),t.forcey&&a.yDomain([0,t.forcey]),a.x(i.xFunction()),a.y(i.yFunction()),a.xAxis.tickFormat(i.xAxisTickFormat()),t.percentage?(e=i.yAxisPercentageTickFormat(),a.yAxis.tickFormat()):t.integer&&(e=i.yAxisIntegerTickFormat(),a.yAxis.tickFormat()),a.yAxis.tickFormat(e),nv.utils.windowResize(a.update),d3.select("#"+t.id+" svg").datum(t.data).style("height",n+"px").transition().duration(0).call(a),a}),t.$on("updateMetrics",function(){a.update()})}return{restrict:"A",templateUrl:"app/components/chart/chart.html",scope:{data:"=",percentage:"=",integer:"=",forcey:"="},link:a}}e.$inject=["$rootScope","$log","D3Service"],angular.module("chart").directive("areaStackedTimeSeries",e)}(),/**!
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
function(){"use strict";function e(e,t,i,a,n,r,o,s,c,l,d,u,p){function m(){e[0].hidden||e[0].webkitHidden||e[0].mozHidden||e[0].msHidden?d.cancelInterval():d.updateInterval()}function g(){if(d.initialize(),n.protocol&&(t.properties.protocol=n.protocol),n.host&&(h.inputHost=n.host,n.hostspec&&(t.properties.hostspec=n.hostspec),d.updateHost(h.inputHost)),e[0].addEventListener("visibilitychange",m,!1),e[0].addEventListener("webkitvisibilitychange",m,!1),e[0].addEventListener("msvisibilitychange",m,!1),e[0].addEventListener("mozvisibilitychange",m,!1),angular.isDefined(n.widgets)){var i=n.widgets.split(",")||[];f=i.reduce(function(e,t){return e.concat(o.filter(function(e){return e.name===t}))},[])}else{var a=s.reduce(function(e,t){return e.push(t.name),e},[]).join();r.search("widgets",a)}angular.isDefined(n.containerFilter)&&(t.properties.containerFilter=n.containerFilter),h.dashboardOptions={hideToolbar:!0,widgetButtons:!1,hideWidgetName:!0,hideWidgetSettings:!1,widgetDefinitions:o,defaultWidgets:f}}var h=this,f=s;h.version=l.id,h.embed=c,h.addWidgetToURL=function(e){var t="";angular.isUndefined(n.widgets)?n.widgets="":t=",",e.length?(n.widgets="",t=e.reduce(function(e,t){return e.push(t.name),e},[]).join()):t+=e.name,r.search("widgets",n.widgets+t)},h.removeWidgetFromURL=function(e){for(var t=n.widgets.split(",")||[],i=0;i<t.length;i++)if(t[i]===e.name){t.splice(i,1);break}t.length<1?h.removeAllWidgetFromURL():r.search("widgets",t.toString())},h.removeAllWidgetFromURL=function(){r.search("widgets",null)},h.resetDashboard=function(){var e={closeButtonText:"Cancel",actionButtonText:"Ok",headerText:"Reset Dashboard",bodyText:"Are you sure you want to reset the dashboard?"};p.showModal({},e).then(function(){r.search("container",null),r.search("containerFilter",null),t.flags.disableContainerSelectNone=!1,t.properties.selectedContainer="",t.properties.containerFilter="",h.dashboardOptions.loadWidgets([]),h.removeAllWidgetFromURL()})},h.updateHost=function(){d.updateHost(h.inputHost),u.clearIdDictionary()},h.addWidget=function(e,t){e.preventDefault(),h.checkWidgetType(t)&&(h.dashboardOptions.addWidget(t),h.addWidgetToURL(t))},h.checkWidgetType=function(i){if(angular.isDefined(i.requireContainerFilter)&&i.requireContainerFilter===!0&&t.flags.disableContainerSelect===!1&&!t.flags.containerSelectOverride&&""===t.properties.selectedContainer){var a={closeButtonText:"",actionButtonText:"Ok",headerText:"Error: Container selection required.",bodyText:"This widget requires a container to be selected. Please select a container and try again."};return p.showModal({},a).then(function(){e.getElementById("selectedContainer").focus()}),!1}return!0},h.updateInterval=d.updateInterval,h.updateContainer=u.updateContainer,h.updateContainerFilter=u.updateContainerFilter,h.inputHost="",g()}e.$inject=["$document","$rootScope","$log","$route","$routeParams","$location","widgetDefinitions","widgets","embed","version","DashboardService","ContainerMetadataService","ModalService"],angular.module("main",["dashboard","widget","containermetadata","modal"]).controller("MainController",e)}(),/**!
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
function(){"use strict";angular.module("vector",["ngRoute","ui.dashboard","toastr","main"])}(),function(){"use strict";angular.module("vector").constant("version",{id:"v1.1.0-3-gc851eac"})}(),/**!
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
function(){"use strict";function e(e){e.when("/",{templateUrl:"app/main/main.html",controller:"MainController",controllerAs:"vm",title:"Vector",reloadOnSearch:!1,resolve:{widgets:["defaultWidgets",function(e){return e}],embed:function(){return!1}}}).when("/embed",{templateUrl:"app/main/main.html",controller:"MainController",controllerAs:"vm",title:"Vector",reloadOnSearch:!1,resolve:{widgets:["defaultWidgets",function(e){return e}],embed:function(){return!0}}}).when("/empty",{templateUrl:"app/main/main.html",controller:"MainController",controllerAs:"vm",title:"Vector",reloadOnSearch:!1,resolve:{widgets:["emptyWidgets",function(e){return e}],embed:function(){return!1}}}).when("/container",{templateUrl:"app/main/main.html",controller:"MainController",controllerAs:"vm",title:"Vector",reloadOnSearch:!1,resolve:{widgets:["containerWidgets",function(e){return e}],embed:function(){return!1}}}).otherwise("/")}e.$inject=["$routeProvider"],angular.module("vector").config(e)}(),/**!
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
function(){"use strict";function e(e,t){var i,a,n=[];for(i=0;i<e.length;i++)a=e[i][t],-1===n.indexOf(a)&&n.push(a);return n}function t(){return function(t,i){return null!==t?e(t,i):void 0}}function i(){return function(e,t){return e.filter(function(e){return e.group===t})}}angular.module("vector").filter("groupBy",t).filter("groupFilter",i)}(),/**!
 *
 *  Copyright 2016 Netflix, Inc.
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
function(){"use strict";angular.module("vector")}(),/**!
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
function(){"use strict";function e(e){e.decorator("$interval",["$delegate",function(e){var t=e.cancel;return e.cancel=function(e){var i=t(e);return i&&e&&(e.isCancelled=!0),i},e}])}e.$inject=["$provide"],angular.module("vector").config(e)}(),/**!
 *
 *  Copyright 2016 Netflix, Inc.
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
function(){"use strict";angular.module("vector").constant("moment",moment)}(),/**!
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
function(){"use strict";function e(e){e.allowHtml=!0,e.timeOut=8e3,e.positionClass="toast-top-right",e.preventDuplicates=!0,e.progressBar=!0}function t(e){e.debugEnabled(!0)}e.$inject=["toastrConfig"],t.$inject=["$logProvider"],angular.module("vector").constant("config",{protocol:"http",port:44323,hostspec:"localhost",interval:"2",window:"2",enableCpuFlameGraph:!1,enableContainerWidgets:!0,disableHostspecInput:!1,disableContainerFilter:!1,disableContainerSelect:!1,containerSelectOverride:!0,useCgroupId:!1,expandHostname:!1,disableHostnameInputContainerSelect:!1}).config(e).config(t)}(),angular.module("vector").run(["$templateCache",function(e){e.put("app/main/main.html",'<div class="navbar navbar-inverse navbar-fixed-top" role=navigation ng-if=!vm.embed><div class=container-fluid><div class=navbar-header><button type=button class=navbar-toggle data-toggle=collapse data-target=.navbar-collapse><span class=sr-only>Toggle navigation</span> <span class=icon-bar></span> <span class=icon-bar></span> <span class=icon-bar></span></button> <a class=navbar-brand href=#/ ><img ng-src=assets/images/vector_owl.png alt="Vector Owl" height=20></a><a class=navbar-brand href="javascript:window.location.reload(true);history.pushState(null, \'\', location.href.split(\'?\')[0]);">Vector</a></div></div></div><div class=dashboard-container ng-class="{ \'main-container\': !vm.embed }"><div class=row><div class=col-md-12><div dashboard=vm.dashboardOptions template-url=app/components/dashboard/dashboard.html class=dashboard-container></div></div></div></div>'),e.put("app/components/chart/chart.html",'<div><i class="fa fa-line-chart fa-5x widget-icon" ng-hide=flags.contextAvailable></i><div id={{id}} class=chart ng-hide=!flags.contextAvailable><svg></svg></div></div>'),e.put("app/components/customWidgetSettings/customWidgetSettings.html",'<div class=modal-header><button type=button class=close data-dismiss=modal aria-hidden=true ng-click=cancel()>&times;</button><h3>Widget Options <small>{{widget.title}}</small></h3></div><div class=modal-body><form name=form novalidate class=form-horizontal><div class=input-group><span class=input-group-addon>Filter</span> <input class="widgetInput form-control" name=widgetFilter ng-model=result.filter ng-change="vm.updateFilterWidget(this.widget, result)" ng-keydown="$event.which === 13 && ok()"></div></form></div><div class=modal-footer><button type=button class="btn btn-default" ng-click="cancel(); vm.clearFilterWidget(this.widget);">Cancel</button> <button type=button class="btn btn-primary" ng-click=ok()>OK</button></div>'),e.put("app/components/dashboard/dashboard.html",'<div class=row ng-if=!vm.embed><div class=col-md-6><form role=form name=form><div class="input-group input-group-lg target" ng-class="{\'has-error\': form.host.$invalid && form.host.$dirty}"><span class=input-group-addon ng-click="flags.isHostnameExpanded = !flags.isHostnameExpanded;">Hostname &nbsp; <i class="fa fa-plus-square-o" ng-if=!flags.isHostnameExpanded></i><i class="fa fa-minus-square-o" ng-if=flags.isHostnameExpanded></i></span> <input type=text class="form-control hostname-input" id=hostnameInput name=host data-content="Please enter the instance hostname. Port can be specified using the <hostname>:<port> format. Expand to change hostspec." rel=popover data-placement=bottom data-trigger=hover data-container=body ng-model=vm.inputHost ng-change=vm.updateHost() ng-model-options="{ updateOn: \'default\', debounce: 1000 }" delay=1000 ng-disabled="flags.contextUpdating == true || ( flags.disableContainerSelectNone && flags.disableHostnameInputContainerSelect)" required placeholder="Instance Hostname"> <i class="fa fa-refresh fa-2x form-control-feedback" ng-if=flags.contextUpdating></i> <i class="fa fa-check fa-2x form-control-feedback" ng-if=flags.contextAvailable></i></div></form></div><div class="btn-group col-md-2 widget-wrapper" id=widgetWrapper><button id=widgetButton type=button class="dropdown-toggle btn btn-lg btn-default btn-block widget-button" data-toggle=dropdown>Widget <span class=caret></span></button><ul class=dropdown-menu role=menu><li class=dropdown-submenu ng-repeat="group in widgetDefs | groupBy: \'group\'"><a ng-click=void(0) data-toggle=dropdown>{{group}}</a><ul class=dropdown-menu><li ng-repeat="widget in widgetDefs | groupFilter: group"><a ng-click="vm.addWidget($event, widget);">{{widget.title}}</a></li></ul></li><li role=presentation class=divider></li><li><a href=javascript:void(0); ng-click="loadWidgets(defaultWidgets); vm.addWidgetToURL(defaultWidgets);">Default Widgets</a></li><li><a href=javascript:void(0); ng-click="loadWidgets(emptyWidgets); vm.removeAllWidgetFromURL();">Clear Widgets</a></li><li role=presentation class=divider></li><li><a href=javascript:void(0); ng-click=vm.resetDashboard();>Reset Dashboard</a></li></ul></div><div class=col-md-2><form role=form><div class="input-group input-group-lg window" ng-class="{\'has-error\': form.window.$invalid && form.window.$dirty}"><span class=input-group-addon>Window</span><select class=form-control name=window id=windowInput data-content="The duration window for all charts in this dashboard." rel=popover data-placement=bottom data-trigger=hover data-container=body ng-model=properties.window ng-change=vm.updateWindow()><option value=2>2 min</option><option value=5>5 min</option><option value=10>10 min</option></select></div></form></div><div class=col-md-2><form role=form><div class="input-group input-group-lg interval" ng-class="{\'has-error\': form.interval.$invalid && form.interval.$dirty}"><span class=input-group-addon>Interval</span><select class=form-control name=interval id=intervalInput data-content="The update interval used by all charts in this dashboard." rel=popover data-placement=bottom data-trigger=hover data-container=body ng-model=properties.interval ng-change=vm.updateInterval()><option value=1>1 sec</option><option value=2>2 sec</option><option value=3>3 sec</option><option value=5>5 sec</option></select></div></form></div></div><div class=row ng-show=flags.isHostnameExpanded ng-if=!vm.embed><div class=col-md-6><form role=form name=form><div class="input-group input-group-lg target" ng-show=!flags.disableHostspecInput ng-class="{\'has-error\': form.hostspec.$invalid && form.hostspec.$dirty}"><span class=input-group-addon>Hostspec</span> <input type=text class=form-control id=hostspecInput name=host data-content="Please enter the instance hostspec." rel=popover data-placement=bottom data-trigger=hover data-container=body ng-model=properties.hostspec ng-change=vm.updateHost() ng-model-options="{ updateOn: \'default\', debounce: 1000 }" delay=1000 ng-disabled="flags.contextUpdating == true || ( flags.disableContainerSelectNone && flags.disableHostnameInputContainerSelect )" required placeholder="Instance Hostspec"></div><div class="input-group input-group-lg target" ng-show="flags.enableContainerWidgets && !flags.disableContainerFilter"><span class=input-group-addon>Container Filter</span> <input type=text class=form-control id=globalFilterInput name=globalFilter data-content="Global filter for container widgets" rel=popover data-placement=bottom data-trigger=hover data-container=body ng-model=properties.containerFilter ng-change=vm.updateContainerFilter() ng-model-options="{ updateOn: \'default\', debounce: 1000 }" delay=1000 placeholder="Container Filter"></div><div class="input-group input-group-lg target" ng-show="flags.enableContainerWidgets && !flags.disableContainerSelect"><span class=input-group-addon>Container Id</span><select id=selectedContainer ng-model=properties.selectedContainer ng-change=vm.updateContainer() class=form-control><option ng-disabled="flags.disableContainerSelectNone && !flags.containerSelectOverride" value="">All</option><option ng-repeat="container in properties.containerList">{{container}}</option></select></div></form></div><div class=col-md-6><p class="lead hostname-label" id=hostnameLabel data-content="PMCD hostname. The hostname from the actual instance you\'re monitoring." rel=popover data-placement=bottom data-trigger=hover data-container=body>{{flags.contextAvailable && properties.hostname ? properties.hostname : \'Disconnected\'}}</p></div></div><div class=row><div class=col-md-12><div class=btn-toolbar ng-if=!options.hideToolbar><div class=btn-group ng-if=!options.widgetButtons><button type=button class="dropdown-toggle btn btn-primary" data-toggle=dropdown>Add Widget <span class=caret></span></button><ul class=dropdown-menu role=menu><li ng-repeat="widget in widgetDefs"><a href=# ng-click="addWidgetInternal($event, widget);"><span class="label label-primary">{{widget.name}}</span></a></li></ul></div><div class=btn-group ng-if=options.widgetButtons><button ng-repeat="widget in widgetDefs" ng-click="addWidgetInternal($event, widget);" type=button class="btn btn-primary">{{widget.name}}</button></div><button class="btn btn-warning" ng-click=resetWidgetsToDefault()>Default Widgets</button> <button ng-if="options.storage && options.explicitSave" ng-click=options.saveDashboard() class="btn btn-success" ng-disabled=!options.unsavedChangeCount>{{ !options.unsavedChangeCount ? "all saved" : "save changes (" + options.unsavedChangeCount + ")" }}</button> <button ng-click=clear(); type=button class="btn btn-info">Clear</button></div><div ui-sortable=sortableOptions ng-model=widgets class=dashboard-widget-area><div ng-repeat="widget in widgets" ng-style=widget.containerStyle class=widget-container widget><div class="widget panel panel-default"><div class="widget-header panel-heading"><h3 class=panel-title><span class=widget-title>{{widget.title}}</span><!-- <span class="widget-title" ng-dblclick="editTitle(widget)" ng-hide="widget.editingTitle">{{widget.title}}</span> --><!-- <form action="" class="widget-title" ng-show="widget.editingTitle" ng-submit="saveTitleEdit(widget)">\n                                <input type="text" ng-model="widget.title" class="form-control">\n                            </form> --> <span class="label label-primary" ng-if=!options.hideWidgetName>{{widget.name}}</span> <span ng-click="removeWidget(widget);  vm.removeWidgetFromURL(widget);" class="glyphicon glyphicon-remove" ng-if=!options.hideWidgetClose></span> <span ng-click=openWidgetSettings(widget); class="glyphicon glyphicon-cog" ng-if="!options.hideWidgetSettings && widget.hasLocalSettings"></span></h3></div><div class="panel-body widget-content" ng-style=widget.contentStyle></div><div class=widget-ew-resizer ng-mousedown=grabResizer($event)></div><div ng-if=widget.enableVerticalResize class=widget-s-resizer ng-mousedown=grabSouthResizer($event)></div></div></div></div></div></div><div class="row version-div" ng-if=!vm.embed>Version: {{vm.version}}</div><script>(function () {\n        \'use strict\';\n        $(\'#hostnameInput\').popover();\n        $(\'#hostspecInput\').popover();\n        $(\'#globalFilterInput\').popover();\n        $(\'#windowInput\').popover();\n        $(\'#intervalInput\').popover();\n        $(\'#hostnameLabel\').popover();\n    }());</script>'),e.put("app/components/flamegraph/flamegraph.html",'<div class=col-md-12><div class=row style="text-align: center"><p>Click on the button below to generate a CPU flame graph! (60 sec)</p><button type=button class="btn btn-primary" ng-disabled=processing ng-click=generateFlameGraph()>Generate Flame Graph <i ng-if=processing class="fa fa-refresh fa-spin"></i></button></div><div class=row ng-if="!processing && ready" style="text-align: center; margin-top: 15px"><p>The CPU flame graph is ready. Please click on the button below to open it.</p><a class="btn btn-default" href=http://{{host}}:{{port}}/systack/systack.svg target=_blank>Open Flame Graph</a></div></div>')}]);
//# sourceMappingURL=../maps/scripts/app-cc8970137d.js.map
