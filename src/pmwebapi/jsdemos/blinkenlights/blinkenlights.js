var pm_host = location.hostname;
var pm_root = "http://" + pm_host + ":44323/pmapi";
var pm_context = -1;

// ----------------------------------------------------------------------

function Predicate(name,index,operator,threshold) {
  this.name = name;
  this.index = index;
  this.operator = operator;
  this.threshold = threshold;
  this.inames = {};
}

Predicate.prototype.to_string = function() {
  return this.name + "[" + this.index + "] "
    + this.operator + " " + this.threshold;
};

Predicate.prototype.get_iname = function(iid) {
  if (!(iid in this.inames)) {
    var pm_url = pm_root + "/" + pm_context + "/_indom?name=" + this.name + "&instance=" + iid;
    var predicate = this;
    $.getJSON(pm_url, function(data, status) {
      // TODOXXX error check: should return 1 instance
      predicate.inames[iid] = data.instances[0].name;
    });

    return "..."; // will be reloaded next cycle
  }

  return this.inames[iid];
}

Predicate.prototype.test = function(elt,data_dict,index) {
  if (this.index == "*" && typeof(index) == "undefined") {
    var predicate = this;
    $.each(data_dict[this.name].instances, function(i,_instance) {
      predicate.test(elt,data_dict,i);
    });
    return;
  }
  
  if (typeof(index) == "undefined") index = this.index;

  var metric = data_dict[this.name].instances[index].value;
  var iid = data_dict[this.name].instances[index].instance;
  var result = 0, error = "";
  if (this.operator == "<") result = metric < this.threshold;
  else if (this.operator == ">") result = metric > this.threshold;
  else if (this.operator == "<=") result = metric <= this.threshold;
  else if (this.operator == ">=") result = metric >= this.threshold;
  else if (this.operator == "==") result = metric == this.threshold;
  else { error = "unknown operator '" + this.operator + "'"; result = -1; }
  
  // TODOXXX avoid $("#blinkenlights").empty() by using existing li's??
  var bclass = result < 0 ? "error" : result ? "on" : "off";

  var source = "<strong>" + metric + "</strong> -- "
             + this.name + " ( " + this.get_iname(iid) + " : " + iid + " ) "
             + this.operator + " " + this.threshold;
  var content = "<span>" + source + "</span>"
              + (result < 0 ? " -- error: " + error : "");

  elt.append("<li class=\"" + bclass + "\">" + content + "</li>");
};

var predicates = [];

function parsePredicate(src) {
  var matches = /^([^[ ]+)\s*(\[\d+\]|\[\*\]|\[\]|)\s*(<=|>=|==|<|>)\s*(\S*)$/.exec(src);
  if (matches == null) return null;
  var name = matches[1];
  var index = matches[2]; index = index == "" ? "*" : index.substring(1,index.length-1);
  var operator = matches[3];
  var threshold = parseFloat(matches[4]); // TODOXXX what about other types?; accepts 40foobar
  if (isNaN(threshold)) return null;

  console.log ("create predicate " + name + " : " + index + " : " + operator + " : " + threshold)

  return new Predicate(name,index,operator,threshold);
}

// ----------------------------------------------------------------------

var updateInterval = 10000; // milliseconds
var updateIntervalID = 1;

function setUpdateInterval(i) {
   if (updateIntervalID >= 0) { clearInterval(updateIntervalID); }
   if (i > updateInterval) { pm_context = -1; } // will likely need a new context
   updateInterval = i;
   updateIntervalID = setInterval(updateBlinkenlights, updateInterval); 
}

// default mode
var pm_context_type = "hostspec";
var pm_context_spec = "localhost";

function setPMContextType(k) {
   pm_context_type = k;
   pm_context = -1;
   updateBlinkenlights();
}
function setPMContextSpec(i) {
   pm_context_spec = i;
   pm_context = -1;
   updateBlinkenlights();
}


function updateBlinkenlights() {
  var pm_url;

  if (pm_context < 0) {
    pm_url = pm_root
          + "/context?"
          + pm_context_type + "=" + encodeURIComponent(pm_context_spec)
          + "&polltimeout=" + Math.floor(5*updateInterval/1000);
    $.getJSON(pm_url, function(data, status) {
      pm_context = data.context;
      setTimeout(updateBlinkenlights, 100); // retry soon
    }).error(function() { pm_context = -1; });
    return; // will retry one cycle later
  }

  if(predicates.length == 0) {
      $("#blinkenlights").html("<b>No predicates requested...</b>");
      return;
  }

  // ajax request for JSON data
  pm_url = pm_root + "/" + pm_context + "/_fetch?names=";
  $.each(predicates, function(i, predicate) {
    if (i > 0) pm_url += ",";
    pm_url += predicate.name;
  });
  $.getJSON(pm_url, function(data, status) {
    // update data_dict
    var data_dict = {};
    $.each(data.values, function(i, value) {
      data_dict[value.name] = value;
    });
    // update status field
    theDate = new Date(0);
    theDate.setUTCSeconds(data.timestamp.s);
    theDate.setUTCMilliseconds(data.timestamp.us/1000);
    $("#status").html("Timestamp: " + theDate.toString());
    // update the view
    $("#blinkenlights").empty();
    $.each(predicates, function(i, predicate) {
      predicate.test($("#blinkenlights"), data_dict);
    });
  }).error(function() { 
      $("#blinkenlights").html("<b>error accessing server, retrying...</b>");
      pm_context = -1; });
}

function loadBlinkenlights() {
  $("#header").html("pcp blinkenlights demo");
  $("#content").html("<ul id=\"blinkenlights\"><li>loading...</li></ul>");

  // start timer for updateBlinkenlights
  updateBlinkenlights();
  setUpdateInterval(updateInterval);
}

$(document).ready(function() {
  loadBlinkenlights();
  // TODOXXX add support for editing mode
});
