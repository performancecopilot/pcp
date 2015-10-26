var pm_root = "http://" + location.hostname + ":" + location.port + "/pmapi";
var pm_context = -1;

// ----------------------------------------------------------------------

var metrics = {};

function Metric(name,index) {
    this.name = name;
    this.index = index;
}

// create a metric to fetch (FIXME pull metrics from functions)
metrics["kernel.all.load"] = new Metric("kernel.all.load", "");
metrics["disk.dm.avactive"] = new Metric("disk.dm.avactive", "");
metrics["hinv.ncpu"] = new Metric("hinv.ncpu", "");
metrics["kernel.percpu.cpu.idle"] = new Metric("kernel.percpu.cpu.idle", "");

Metric.prototype.to_string = function() {
    return this.name + "[" + this.index + "]";
};

Metric.prototype.get_iname = function(iid) {
  if (!(iid in this.inames)) {
    var pm_url = pm_root + "/" + pm_context + "/_indom?name=" + this.name + "&instance=" + iid;
    var metric = this;
    $.getJSON(pm_url, function(data, status) {
      // TODOXXX error check: should return 1 instance
      metric.inames[iid] = data.instances[0].name;
    });

    return "..."; // will be reloaded next cycle
  }

  return this.inames[iid];
}

// timestamps are recorded in milliseconds
var data_timestamp;
var old_data_timestamp;
var data_dict = {};
var old_data_dict = {};

function fetch_metric(name, i) {
    console.log( name + "[" + i + "] = " + data_dict[name].instances[i].value);
    return data_dict[name].instances[i].value;
}

function fetch_metric_rate(name, i) {
    var xs = old_data_dict[name].instances[i].value
    var xe = data_dict[name].instances[i].value
    var r = (xe-xs)/(data_timestamp-old_data_timestamp);
    console.log( "rate " + name + "[" + i + "] = " + r);
    return r;
}

function fetch_metric_max(name) {
    var m = fetch_metric_rate(name, 0);
    for (var i = 1, length = data_dict[name].instances.length; i < length; i++) {
        m = Math.max(m, fetch_metric_rate(name, i));
    }
    return m;
}

function fetch_metric_min(name) {
    var m = fetch_metric_rate(name, 0);
    for (var i = 1, length = data_dict[name].instances.length; i < length; i++) {
        m = Math.min(m, fetch_metric_rate(name, i));
    }
    return m;
}


// ----------------------------------------------------------------------


function Node(exp, desc) {
    this.exp = exp; // the test to determine whether to check this tree
    this.desc = desc; // the description of what it is doing
    this.score = 0; // the current value of the node
    this.parent = null;
    this.children = []; // list of possible contributors
}

function Tree(exp,desc) {
    var node = new Node(exp,desc);
    this._root = node;
}

function addChild(parent,child) {
    parent.children.push(child);
    child.parent = parent;
}

Tree.prototype.traverseDF = function(pre,post,res) {
    return (function recurse(currentNode, level, res) {
        res = pre(currentNode, level, res);
        for (var i = 0, length = currentNode.children.length; i < length; i++) {
            res = recurse(currentNode.children[i], level+1, res);
        }
        res = post(currentNode, level, res);
	return res ;
    })(this._root, 0, res)
};

/*
  The following priority heap code is from:
  http://eloquentjavascript.net/1st_edition/appendix2.html
  From http://eloquentjavascript.net/:

Written by Marijn Haverbeke.

Licensed under a Creative Commons attribution-noncommercial
license. All code in this book may also be considered licensed under
an MIT license.

*/

function BinaryHeap(scoreFunction){
  this.content = [];
  this.scoreFunction = scoreFunction;
}

BinaryHeap.prototype = {
  push: function(element) {
    // Add the new element to the end of the array.
    this.content.push(element);
    // Allow it to bubble up.
    this.bubbleUp(this.content.length - 1);
  },

  pop: function() {
    // Store the first element so we can return it later.
    var result = this.content[0];
    // Get the element at the end of the array.
    var end = this.content.pop();
    // If there are any elements left, put the end element at the
    // start, and let it sink down.
    if (this.content.length > 0) {
      this.content[0] = end;
      this.sinkDown(0);
    }
    return result;
  },

  remove: function(node) {
    var length = this.content.length;
    // To remove a value, we must search through the array to find
    // it.
    for (var i = 0; i < length; i++) {
      if (this.content[i] != node) continue;
      // When it is found, the process seen in 'pop' is repeated
      // to fill up the hole.
      var end = this.content.pop();
      // If the element we popped was the one we needed to remove,
      // we're done.
      if (i == length - 1) break;
      // Otherwise, we replace the removed element with the popped
      // one, and allow it to float up or sink down as appropriate.
      this.content[i] = end;
      this.bubbleUp(i);
      this.sinkDown(i);
      break;
    }
  },

  size: function() {
    return this.content.length;
  },

  bubbleUp: function(n) {
    // Fetch the element that has to be moved.
    var element = this.content[n], score = this.scoreFunction(element);
    // When at 0, an element can not go up any further.
    while (n > 0) {
      // Compute the parent element's index, and fetch it.
      var parentN = Math.floor((n + 1) / 2) - 1,
      parent = this.content[parentN];
      // If the parent has a lesser score, things are in order and we
      // are done.
      if (score >= this.scoreFunction(parent))
        break;

      // Otherwise, swap the parent with the current element and
      // continue.
      this.content[parentN] = element;
      this.content[n] = parent;
      n = parentN;
    }
  },

  sinkDown: function(n) {
    // Look up the target element and its score.
    var length = this.content.length,
    element = this.content[n],
    elemScore = this.scoreFunction(element);

    while(true) {
      // Compute the indices of the child elements.
      var child2N = (n + 1) * 2, child1N = child2N - 1;
      // This is used to store the new position of the element,
      // if any.
      var swap = null;
      // If the first child exists (is inside the array)...
      if (child1N < length) {
        // Look it up and compute its score.
        var child1 = this.content[child1N],
        child1Score = this.scoreFunction(child1);
        // If the score is less than our element's, we need to swap.
        if (child1Score < elemScore)
          swap = child1N;
      }
      // Do the same checks for the other child.
      if (child2N < length) {
        var child2 = this.content[child2N],
        child2Score = this.scoreFunction(child2);
        if (child2Score < (swap == null ? elemScore : child1Score))
          swap = child2N;
      }

      // No need to swap further, we are done.
      if (swap == null) break;

      // Otherwise, swap and continue.
      this.content[n] = this.content[swap];
      this.content[swap] = element;
      n = swap;
    }
  }
};

function forEach(array, action) {
  for (var i = 0; i < array.length; i++)
    action(array[i]);
}

function method(object, name) {
  return function() {
    return object[name].apply(object, arguments);
  };
}

/*
  The predicates for each node range from 0 (not possible) to 1.0 (a certainty).
  The search will start at the root node and recursively expand the search 
  The weight of each node is obtained using product of the weights on the path to the node.
  Nodes will be searched for highest to lowest weight.
*/

// code to do a breath first search of the tree to see what things are
// causing the problem.
function search(root)
{
    // initialize the queue
    var heap = new BinaryHeap(function(x){return -(x.score);});
    // start at the root
    root.score = root.exp();
    heap.push(root);
    while (heap.size() > 0) {
	var currentNode = heap.pop();
	console.log("score:" + currentNode.score + " exp:" + currentNode.exp()
		    + " " + currentNode.desc);
	//   foreach child
	for (var i = 0, length = currentNode.children.length; i < length; i++) {
	    //      evaluate the score (exp * prev lev score)
	    currentNode.children[i].score = currentNode.children[i].exp() * currentNode.score;
	    if (currentNode.children[i].score > 0) {
		heap.push(currentNode.children[i]);
	    }
	}
    }
}


/* FIXME: the predicate weights need to be replaced with actual code to get the weights */

var tree = new Tree(function() {return 1.0}, 'toplev of checklist');

cpu = new Node(function() {return (1- fetch_metric_min("kernel.percpu.cpu.idle")); }, 'cpu limited');
addChild(tree._root, cpu);

serialization = new Node(function() {return 0.5;}, 'poor explotation of parallelism')
addChild(cpu, serialization);
thread_limited = new Node(
    function() { return Math.min(1.0, fetch_metric("kernel.all.load",0)/fetch_metric("hinv.ncpu",0));},
    'runnable threads > processors');
addChild(cpu, thread_limited);
cpu_mem = new Node(function() {return 0.5;}, 'poor memory system performance');
addChild(cpu, cpu_mem);
cpu_task_migration = new Node(function() {return 0.5;}, 'excessive task migration');
addChild(cpu_mem, cpu_task_migration);
cpu_task_interfere = new Node(function() {return 0.5;}, 'excessive task interference');
addChild(cpu_mem, cpu_task_interfere);
cpu_cache = new Node(function() {return 0.5;}, 'CPU cache issues');
addChild(cpu_mem, cpu_cache);
cache_capacity = new Node(function() {return 0.5;}, 'Cache capacity');
addChild(cpu_cache, cache_capacity);
cache_false_share = new Node(function() {return 0.5;}, 'Cache false sharing');
addChild(cpu_cache, cache_false_share);
tlb = new Node(function() {return 0.5;}, 'Translation Look aside Buffer (TLB) issues');
addChild(cpu_mem, tlb);
thp_issue = new Node(function() {return 0.5;}, 'Transparent Huge Page issue');
addChild(tlb, thp_issue);
itlb_issue = new Node(function() {return 0.5;}, 'Instruction TLB issue');
addChild(tlb, itlb_issue);
numa_layout = new Node(function() {return 0.5;}, 'NUMA memory layout')
addChild(cpu_mem, numa_layout);
branch_miss = new Node(function() {return 0.5;}, 'Branch misprediction');
addChild(cpu, branch_miss);


mem = new Node(function() {return 0.5;}, 'memory limited');
addChild(tree._root, mem);

net = new Node(function(value) {return 0.5;}, 'network limited');
addChild(tree._root, net);
tx_bw = new Node(function() {return 0.5;}, 'tx-bandwidth limited');
addChild(net, tx_bw);
rx_bw = new Node(function() {return 0.5;}, 'rx-bandwidth limited');
addChild(net, rx_bw);
buff_bloat = new Node(function() {return 0.5;}, 'buffer bloat');
addChild(net, buff_bloat);

storage = new Node(function() {return 0.5;}, 'storage limited');
addChild(tree._root, storage);


// ----------------------------------------------------------------------

var updateInterval = 10000; // milliseconds
var updateIntervalID = 1;

function setUpdateInterval(i) {
   if (updateIntervalID >= 0) { clearInterval(updateIntervalID); }
   if (i > updateInterval) { pm_context = -1; } // will likely need a new context
   updateInterval = i;
   updateIntervalID = setInterval(updateChecklist, updateInterval);
}

// default mode
var pm_context_type = "hostspec";
var pm_context_spec = "localhost";

function setPMContextType(k) {
   pm_context_type = k;
   pm_context = -1;
   updateChecklist();
}
function setPMContextSpec(i) {
   pm_context_spec = i;
   pm_context = -1;
   updateChecklist();
}


function updateChecklist() {
  var pm_url;

    if (pm_context < 0) {
	pm_url = pm_root
            + "/context?"
            + pm_context_type + "=" + encodeURIComponent(pm_context_spec)
            + "&polltimeout=" + Math.floor(5*updateInterval/1000);
	$.getJSON(pm_url, function(data, status) {
	    pm_context = data.context;
	    setTimeout(updateChecklist, 100); // retry soon
	}).error(function() { pm_context = -1; });
	return; // will retry one cycle later
    }

    if(metrics.length == 0) {
	$("#content").html("<b>No metrics requested...</b>");
	return;
    }

    // ajax request for JSON data
    pm_url = pm_root + "/" + pm_context + "/_fetch?names=";
    var i = 0;
    for(var m in metrics) {
	if (i > 0) pm_url += ",";
	pm_url += metrics[m].name;
	++i;
    };

    // Copy current metrics over old metrics
    for (var k in data_dict) {
	old_data_dict[k] = data_dict[k];
    }
    old_data_timestamp = data_timestamp;
    data_dict = {};

    $.getJSON(pm_url, function(data, status) {
	// update data_dict
	$.each(data.values, function(i, value) {
	    data_dict[value.name] = value;
	    console.log( "data_dict[" + value.name + "] = "); console.log(data_dict[value.name]);
	});
	data_timestamp = data.timestamp.s * 1000 + (data.timestamp.us/1000);
	// update status field
	theDate = new Date(0);
	theDate.setUTCSeconds(data.timestamp.s);
	theDate.setUTCMilliseconds(data.timestamp.us/1000);
	$("#status").html("Timestamp: " + theDate.toString());

	// do the search to find out the likely problem
	search(tree._root);
    }).error(function() {
	$("#checklist").html("<b>error accessing server, retrying...</b>");
	pm_context = -1; });
}


function loadChecklist() {
    $("#header").html("pcp checklist demo");
    $("#content").html(
    tree.traverseDF(
	function(node, level, o) { return (o + "<ul><li>" + node.desc + "\n");},
	function(node, level, o) { return (o + "</li></ul>\n");},
	""));

    // start timer for updateChecklist
    updateChecklist();
    setUpdateInterval(updateInterval);
}

$(document).ready(function() {
  loadChecklist();
});
