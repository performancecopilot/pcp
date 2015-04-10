'use strict';

describe('Factory: DashboardState', function () {

  // instantiate service
  var 
    $q,
    DashboardState,
    state,
    state_no_storage,
    state_no_stringify,
    storage,
    id,
    hash,
    widgetDefinitions,
    item,
    setItemSpy,
    getItemSpy,
    removeItemSpy,
    $log,
    $rootScope;

  // load the service's module
  beforeEach(module('ui.dashboard', function($provide) {
    $log = {
      log: function() {},
      warn: function() {},
      info: function() {},
      error: function() {},
      debug: function() {}
    };
    $provide.value('$log', $log);
  }));
  beforeEach(inject(function (_DashboardState_, _$q_, _$rootScope_) {
    DashboardState = _DashboardState_;
    $q = _$q_;
    $rootScope = _$rootScope_;
    
    storage = {
        setItem: function(key, item) {

        },
        getItem: function() {
            return item;
        },
        removeItem: function() {

        }
    };

    id = 'myDashId';

    hash = 'someHash';

    widgetDefinitions = {
        getByName: function(name) {
          return { name: name };
        }
    };

    state = new DashboardState(storage, id, hash, widgetDefinitions, true);
    state_no_storage = new DashboardState(undefined, undefined, undefined, widgetDefinitions, true);
    state_no_stringify = new DashboardState(storage, id, hash, widgetDefinitions, false);

  }));

  it('should have save and load methods', function(){
    expect(typeof DashboardState.prototype.load).toEqual('function');
    expect(typeof DashboardState.prototype.save).toEqual('function');
  });

  it('should set the arguments passed to it in the constructor on the state instance', function() {
    expect(state.storage).toEqual(storage);
    expect(state.id).toEqual(id);
    expect(state.hash).toEqual(hash);
    expect(state.widgetDefinitions).toEqual(widgetDefinitions);
  });

  describe('the save function', function() {
    
    it('should return true if storage is not defined', function() {
      expect(state_no_storage.save()).toEqual(true);
    });

    it('should serialize and store widgets passed to it', function() {
      spyOn(storage, 'setItem');
      var widgets = [
        { title: 'widget1', name: 'Widget1', size: { width: '50%' }, dataModelOptions: { foo: 'bar' }, storageHash: '123', attrs: { bar: 'baz' } },
        { title: 'widget2', name: 'Widget2', size: { width: '50%' }, dataModelOptions: { foo: 'bar' }, storageHash: '123'},
        { title: 'widget3', name: 'Widget3', size: { width: '100%' }, attrs: { bar: 'baz' } },
        { title: 'widget4', name: 'Widget3', dataModelOptions: { foo: 'baz' }, storageHash: '123', arbitrary: 'value' }
      ];
      state.save(widgets);
      delete widgets[3].arbitrary;
      expect(storage.setItem).toHaveBeenCalledWith(id, JSON.stringify({widgets: widgets, hash: hash}));
    });

    it('should not use JSON.stringify if options.stringifyStorage is false', function() {
      spyOn(storage, 'setItem');
      var widgets = [
        { title: 'widget1', name: 'Widget1', size: { width: '50%' }, dataModelOptions: { foo: 'bar' }, storageHash: '123', attrs: { bar: 'baz' } },
        { title: 'widget2', name: 'Widget2', size: { width: '50%' }, dataModelOptions: { foo: 'bar' }, storageHash: '123'},
        { title: 'widget3', name: 'Widget3', size: { width: '100%' }, attrs: { bar: 'baz' } },
        { title: 'widget4', name: 'Widget3', dataModelOptions: { foo: 'baz' }, storageHash: '123' }
      ];
      state_no_stringify.save(widgets);
      expect(typeof storage.setItem.calls.argsFor(0)[1]).toEqual('object');
    });

  });

  describe('the load function', function() {

    var serialized;

    beforeEach(function() {
        serialized = {
          widgets: [ {name: 'W1'}, {name: 'W2'}, {name: 'W3'} ],
          hash: hash
        };
    });

    it('should return null if storage is disabled', function() {
      expect(state_no_storage.load()).toEqual(null);
    });

    it('should return null if storage is enabled but no value was found', function() {
      spyOn(storage, 'getItem').and.returnValue(null);
      expect(state.load() === null).toEqual(true);
    });

    it('should return null and log a warning if the stored string is invalid JSON', function() {
      var malformed = JSON.stringify(serialized).replace(/\}$/,'');
      spyOn(storage, 'getItem').and.returnValue(malformed);
      spyOn($log, 'warn');
      var res = state.load();
      expect(res).toEqual(null);
      expect($log.warn).toHaveBeenCalled();
    });

    it('should return an array of widgets if getItem returns valid serialized dashboard state', function() {
      spyOn(storage, 'getItem').and.returnValue(JSON.stringify(serialized));
      var res = state.load();
      expect(res instanceof Array).toEqual(true);
      expect(res).toEqual(serialized.widgets);
    });

    it('should return null if getItem returned a falsey value', function() {
      var spy = spyOn(storage, 'getItem').and;
      spy.returnValue(false);
      expect(state.load()).toEqual(null);
      spy.returnValue(null);
      expect(state.load()).toEqual(null);
      spy.returnValue(undefined);
      expect(state.load()).toEqual(null);
    });

    it('should return a promise if getItem returns a promise', function() {
      var deferred = $q.defer();
      spyOn(storage, 'getItem').and.returnValue(deferred.promise);
      var res = state.load();
      expect(typeof res.then).toEqual('function');
    });

    it('should return null if the stored hash is different from current hash', function() {
      serialized.hash = 'notTheSame';
      spyOn(storage, 'getItem').and.returnValue(JSON.stringify(serialized));
      spyOn($log, 'info');
      var res = state.load();
      expect(res).toEqual(null);
      expect($log.info).toHaveBeenCalled();
    });

    it('should not include widgets that have no corresponding WDO', function() {
      spyOn(widgetDefinitions, 'getByName').and.returnValue(false);
      spyOn($log, 'warn');
      spyOn(storage, 'getItem').and.returnValue(JSON.stringify(serialized));
      var res = state.load();
      expect(res).toEqual([]);
      expect($log.warn.calls.count()).toEqual(3);
    });

    it('should not include widgets who have a stale hash', function() {
      serialized.widgets[2].storageHash = 'something';
      spyOn($log, 'info');
      spyOn(storage, 'getItem').and.returnValue(JSON.stringify(serialized));
      spyOn(widgetDefinitions, 'getByName').and.callFake(function(name) {
        if (name === 'W3') {
          return { name: 'W3', storageHash: 'else' };
        } else {
          return { name: name };
        }
      });

      var res = state.load();
      expect(res).toEqual(serialized.widgets.slice(0,2));
      expect($log.info).toHaveBeenCalled();
    });

    describe('the loadPromise (returned from load method)', function() {

      var getItemDeferred, getItemPromise, loadPromise;

      beforeEach(function() {
        getItemDeferred = $q.defer();
        getItemPromise = getItemDeferred.promise;
        spyOn(storage, 'getItem').and.returnValue(getItemPromise);
        loadPromise = state.load();
      });

      it('should resolve when the getItem promise resolves with a value', function() {
        var retval;
        loadPromise.then(function(value) {
          retval = value;
        });
        getItemDeferred.resolve( JSON.stringify(serialized) );
        $rootScope.$apply();

        expect(retval).not.toBeUndefined();
      });

      it('should reject when the getItemPromise rejects', function() {
        var failed;
        loadPromise.then(
          function() {
            // success
          },
          function(value) {
            failed = true;
          }
        );
        
        getItemDeferred.reject();

        $rootScope.$apply();

        expect(failed).toEqual(true);
      });

      it('should reject when the getItem promise resolves with a falsey (or no) value', function() {
        var failed;
        loadPromise.then(
          function() {
            // success
          },
          function(value) {
            failed = true;
          }
        );
        
        getItemDeferred.resolve( null );

        $rootScope.$apply();

        expect(failed).toEqual(true);
      });

    });

    it('should use JSON.parse if options.stringifyStorage is true', function() {
      var parse = JSON.parse;
      spyOn(JSON, 'parse').and.callFake(parse);
      spyOn(storage, 'getItem').and.returnValue(JSON.stringify(serialized));
      spyOn(widgetDefinitions, 'getByName').and.callFake(function(name) {
        if (name === 'W3') {
          return { name: 'W3', storageHash: 'else' };
        } else {
          return { name: name };
        }
      });

      var res = state.load();
      expect(JSON.parse).toHaveBeenCalled();
    });

    it('should not use JSON.parse if options.stringifyStorage is false', function() {
      var parse = JSON.parse;
      spyOn(JSON, 'parse').and.callFake(parse);
      spyOn(storage, 'getItem').and.returnValue(serialized);
      spyOn(widgetDefinitions, 'getByName').and.callFake(function(name) {
        if (name === 'W3') {
          return { name: 'W3', storageHash: 'else' };
        } else {
          return { name: name };
        }
      });

      var res = state_no_stringify.load();
      expect(JSON.parse).not.toHaveBeenCalled();
    });

  });

});