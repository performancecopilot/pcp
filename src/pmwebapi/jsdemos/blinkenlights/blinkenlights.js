var pm_host = location.hostname;
var pm_root = "http://" + pm_host + ":44323/pmapi";
var pm_context = -1;

// ----------------------------------------------------------------------

// TODOXXX use actual pictures for the blinkenlights
var red_light = "(!)";
var green_light = "(X)";
var gray_light = "( )";

// ----------------------------------------------------------------------

function Predicate(name,index,operator,threshold) {
  this.name = name;
  this.index = index;
  this.operator = operator;
  this.threshold = threshold;
  this.inames = {}; // TODOXXX gather instance names for this metric, requires _indom support
}

Predicate.prototype.test = function(elt,data_dict,index) {
  if (this.index == "*" && typeof(index) == "undefined") {
    var predicate = this;
    $.each(data_dict[this.name].instances, function(i,_instance) {
      predicate.test(elt,data_dict,i);
    });
    return;
  }
  
  if (typeof(index) == "undefined") index == this.index;

  var metric = data_dict[this.name].instances[index].value;
  var result = 0, error = "";
  if (this.operator == "<") result = metric < this.threshold;
  else if (this.operator == ">") result = metric > this.threshold;
  else if (this.operator == "<=") result = metric <= this.threshold;
  else if (this.operator == ">=") result = metric >= this.threshold;
  else if (this.operator == "==") result = metric == this.threshold;
  else { error = "unknown operator '" + this.operator + "'"; result = -1; }
  
  // TODOXXX assign id to this li, avoid $("#blinkenlights").empty()
  // XXX var blinken_id = 
  var blinken = result < 0 ? red_light : result ? green_light : gray_light;

  // TODOXXX use instance names to display something nicer than an index
  var source = this.name + "[" + index + "] " + this.operator + " " + this.threshold;
  var content = blinken + " <span>" + source + "</span>"
              + (result < 0 ? " &ndash; error: " + error : "");

  // XXX if (elt.find("#" + blinken_id)) ...
  // XXX else elt.append("<li id=\"" + blinken_id + "\">" + content + "</li>")
  elt.append("<li>" + content + "</li>");
};

var predicates = [];

function parsePredicate(src) {
  var matches = /([^[]+)\s*(\[\*\]|\[\]|)\s*(<|>|<=|>=|==)\s*(\S+)/.exec(src);
  var name = matches[1];
  var index = matches[2]; index = index == "" ? "*" : index.substring(1,index.length-1);
  var operator = matches[3];
  var threshold = parseFloat(matches[4]); // TODOXXX what about other types?
  return new Predicate(name,index,operator,threshold);
}

// ----------------------------------------------------------------------

var updateInterval;

function updateBlinkenlights() {
  var pm_url;

  if (pm_context < 0) {
    pm_url = pm_root + "/context?hostname=" + pm_host;
    $.getJSON(pm_url, function(data, status) {
      // TODOXXX error handling
      pm_context = data.context;
      return; // will retry one cycle later
    });
  }

  // ajax request for JSON data
  pm_url = pm_root + "/" + pm_context + "/_fetch?names=";
  $.each(predicates, function(i, predicate) {
    if (i > 0) pm_url += ",";
    pm_url += predicate.name;
  });
  $.getJSON(pm_url, function(data, status) {
    // TODOXXX error handling

    // update data_dict
    var data_dict = {};
    $.each(data.values, function(i, value) {
      data_dict[value.name] = value;
    });

    // update the view
    $("#blinkenlights").empty();
    console.log(data_dict);
    $.each(predicates, function(i, predicate) {
      predicate.test($("#blinkenlights"), data_dict);
    });
  });
}

function loadBlinkenlights() {
  $("#header").html("pcp blinkenlights demo");
  $("#content").html("<ul id=\"blinkenlights\"><li>loading...</li></ul>");

  // start timer for updateBlinkenlights
  updateInterval = setInterval(updateBlinkenlights, 1000);
  updateBlinkenlights();
}

$(document).ready(function() {
  predicates.push(parsePredicate("kernel.all.load[*] > 0.2"));
  // predicates.push(new Predicate("kernel.all.load", "*", ">", 0.2));
  
  loadBlinkenlights();
  // TODOXXX add support for editing mode
});
