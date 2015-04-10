'use strict';

describe('Factory: LayoutStorage', function () {

  // mock UI Sortable
  beforeEach(function () {
    angular.module('ui.sortable', []);
  });

  // load the service's module
  beforeEach(module('ui.dashboard'));

  // instantiate service
  var LayoutStorage;
  beforeEach(inject(function (_LayoutStorage_) {
    LayoutStorage = _LayoutStorage_;
  }));

  describe('the constructor', function() {
    
    var storage, options;

    beforeEach(function() {
      options = {
        storageId: 'testingStorage',
        storage: {
          setItem: function(key, value) {

          },
          getItem: function(key) {

          },
          removeItem: function(key) {

          }
        },
        storageHash: 'ds5f9d1f',
        stringifyStorage: true,
        widgetDefinitions: [

        ],
        defaultLayouts: [
          {title: 'something'},
          {title: 'something'},
          {title: 'something'}
        ],
        widgetButtons: false,
        explicitSave: false,
        settingsModalOptions: {},
        onSettingsClose: function() {

        },
        onSettingsDismiss: function() {

        }
      };
      storage = new LayoutStorage(options);
    });

    it('should provide an empty implementation of storage if it is not provided', function() {
      delete options.storage;
      var stateless = new LayoutStorage(options);
      var noop = stateless.storage;
      angular.forEach(['setItem', 'getItem', 'removeItem'], function(method) {
        expect(typeof noop[method]).toEqual('function');
        expect(noop[method]).not.toThrow();
        noop[method]();
      });
    });

    it('should set a subset of the options directly on the LayoutStorage instance itself', function() {
      var properties = {
        id: 'storageId',
        storage: 'storage',
        storageHash: 'storageHash',
        stringifyStorage: 'stringifyStorage',
        widgetDefinitions: 'widgetDefinitions',
        defaultLayouts: 'defaultLayouts',
        widgetButtons: 'widgetButtons',
        explicitSave: 'explicitSave',
        settingsModalOptions: 'settingsModalOptions',
        onSettingsClose: 'onSettingsClose',
        onSettingsDismiss: 'onSettingsDismiss'
      };

      angular.forEach(properties, function(val, key) {
        expect( storage[key] ).toEqual( options[val] );
      });

    });

    it('should set stringify as true by default', function() {
      delete options.stringifyStorage;
      storage = new LayoutStorage(options);
      expect(storage.stringifyStorage).toEqual(true);
    });

    it('should allow stringify to be overridden by option', function() {
      options.stringifyStorage = false;
      storage = new LayoutStorage(options);
      expect(storage.stringifyStorage).toEqual(false);
    });

    it('should create a layouts array and states object', function() {
      expect(storage.layouts instanceof Array).toEqual(true);
      expect(typeof storage.states).toEqual('object');
    });

    it('should call load', function() {
      spyOn(LayoutStorage.prototype, 'load');
      storage = new LayoutStorage(options);
      expect(LayoutStorage.prototype.load).toHaveBeenCalled();
    });

  });

  describe('the load method', function() {

    var options, storage;

    beforeEach(function() {
      options = {
        storageId: 'testingStorage',
        storage: {
          setItem: function(key, value) {

          },
          getItem: function(key) {

          },
          removeItem: function(key) {

          }
        },
        storageHash: 'ds5f9d1f',
        stringifyStorage: true,
        widgetDefinitions: [

        ],
        defaultLayouts: [
          {title: 'something'},
          {title: 'something'},
          {title: 'something'}
        ],
        widgetButtons: false,
        explicitSave: false
      }
      storage = new LayoutStorage(options);
    });

    it('should use the default layouts if no stored info was found', function() {
      expect(storage.layouts.length).toEqual(options.defaultLayouts.length);
    });

    it('should clone default layouts rather than use them directly', function() {
      expect(storage.layouts.indexOf(options.defaultLayouts[0])).toEqual(-1);
    });

    it('should use the result from getItem for layouts.', function() {
      spyOn(options.storage, 'getItem').and.returnValue(JSON.stringify({
        storageHash: 'ds5f9d1f',
        layouts: [
          { id: 0, title: 'title', defaultWidgets: [], active: true },
          { id: 1, title: 'title2', defaultWidgets: [], active: false },
          { id: 2, title: 'title3', defaultWidgets: [], active: false },
          { id: 3, title: 'custom', defaultWidgets: [], active: false }
        ],
        states: {
          0: {},
          1: {},
          2: {}
        }
      }));
      storage.load();
      expect(storage.layouts.map(function(l) {return l.title})).toEqual(['title', 'title2', 'title3', 'custom']);
    });

    it('should NOT use result from getItem for layouts if the storageHash doesnt match', function() {
      spyOn(options.storage, 'getItem').and.returnValue(JSON.stringify({
        storageHash: 'alskdjf02iej',
        layouts: [
          { id: 0, title: 'title', defaultWidgets: [], active: true },
          { id: 1, title: 'title2', defaultWidgets: [], active: false },
          { id: 2, title: 'title3', defaultWidgets: [], active: false },
          { id: 3, title: 'custom', defaultWidgets: [], active: false }
        ],
        states: {
          0: {},
          1: {},
          2: {}
        }
      }));
      storage.load();
      expect(storage.layouts.map(function(l) {return l.title})).toEqual(['something', 'something', 'something']);
    });

    it('should be able to handle async loading via promise', inject(function($rootScope,$q) {
      var deferred = $q.defer();
      spyOn(options.storage, 'getItem').and.returnValue(deferred.promise);
      storage.load();
      expect(storage.layouts).toEqual([]);
      deferred.resolve(JSON.stringify({
        storageHash: 'ds5f9d1f',
        layouts: [
          { id: 0, title: 'title', defaultWidgets: [], active: true },
          { id: 1, title: 'title2', defaultWidgets: [], active: false },
          { id: 2, title: 'title3', defaultWidgets: [], active: false },
          { id: 3, title: 'custom', defaultWidgets: [], active: false }
        ],
        states: {
          0: {},
          1: {},
          2: {}
        }
      }));
      $rootScope.$apply();
      expect(storage.layouts.map(function(l) {return l.title})).toEqual(['title', 'title2', 'title3', 'custom']);
    }));

    it('should load defaults if the deferred is rejected', inject(function($rootScope,$q) {
      var deferred = $q.defer();
      spyOn(options.storage, 'getItem').and.returnValue(deferred.promise);
      storage.load();
      deferred.reject();
      $rootScope.$apply();
      expect(storage.layouts.map(function(l) {return l.title})).toEqual(['something', 'something', 'something']);
    }));

    it('should load defaults if the json is malformed', inject(function($rootScope,$q) {
      var deferred = $q.defer();
      spyOn(options.storage, 'getItem').and.returnValue(deferred.promise);
      storage.load();
      expect(storage.layouts).toEqual([]);
      deferred.resolve(JSON.stringify({
        storageHash: 'ds5f9d1f',
        layouts: [
          { id: 0, title: 'title', defaultWidgets: [], active: true },
          { id: 1, title: 'title2', defaultWidgets: [], active: false },
          { id: 2, title: 'title3', defaultWidgets: [], active: false },
          { id: 3, title: 'custom', defaultWidgets: [], active: false }
        ],
        states: {
          0: {},
          1: {},
          2: {}
        }
      }).replace('{','{{'));
      $rootScope.$apply();
      expect(storage.layouts.map(function(l) {return l.title})).toEqual(['something', 'something', 'something']);
    }));

    it('should not try to JSON.parse the result if stringifyStorage is false.', function() {
      options.stringifyStorage = false;
      storage = new LayoutStorage(options);
      spyOn(options.storage, 'getItem').and.returnValue({
        storageHash: 'ds5f9d1f',
        layouts: [
          { id: 0, title: 'title', defaultWidgets: [], active: true },
          { id: 1, title: 'title2', defaultWidgets: [], active: false },
          { id: 2, title: 'title3', defaultWidgets: [], active: false },
          { id: 3, title: 'custom', defaultWidgets: [], active: false }
        ],
        states: {
          0: {},
          1: {},
          2: {}
        }
      });
      storage.load();
      expect(storage.layouts.map(function(l) {return l.title})).toEqual(['title', 'title2', 'title3', 'custom']);
    });

  });

  describe('the add method', function() {
    
    var storage, options;

    beforeEach(function() {
      options = {
        storageId: 'testingStorage',
        storage: {
          setItem: function(key, value) {

          },
          getItem: function(key) {

          },
          removeItem: function(key) {

          }
        },
        storageHash: 'ds5f9d1f',
        stringifyStorage: true,
        widgetDefinitions: [

        ],
        defaultLayouts: [],
        widgetButtons: false,
        explicitSave: false
      }

      spyOn(LayoutStorage.prototype, 'load' );

      storage = new LayoutStorage(options);

    });

    it('should add to storage.layouts', function() {
      var newLayout = { title: 'my-layout' };
      storage.add(newLayout);
      expect(storage.layouts[0]).toEqual(newLayout);
    });

    it('should be able to take an array of new layouts', function() {
      var newLayouts = [ { title: 'my-layout' }, { title: 'my-layout-2' } ];
      storage.add(newLayouts);
      expect(storage.layouts.length).toEqual(2);
      expect(storage.layouts.indexOf(newLayouts[0])).not.toEqual(-1);
      expect(storage.layouts.indexOf(newLayouts[1])).not.toEqual(-1);
    });

    it('should look for defaultWidgets on storage options if not supplied on layout definition', function() {
      options.defaultWidgets = [{name: 'a'}, {name: 'b'}, {name: 'c'}];
      storage = new LayoutStorage(options);

      var newLayouts = [ { title: 'my-layout', defaultWidgets: [] }, { title: 'my-layout-2' } ];
      storage.add(newLayouts);
      expect(newLayouts[0].dashboard.defaultWidgets === newLayouts[0].defaultWidgets).toEqual(true);
      expect(newLayouts[1].dashboard.defaultWidgets === options.defaultWidgets).toEqual(true);
    });

    it('should use defaultWidgets if supplied in the layout definition', function() {
      options.defaultWidgets = [{name: 'a'}, {name: 'b'}, {name: 'c'}];
      storage = new LayoutStorage(options);

      var newLayouts = [ { title: 'my-layout', defaultWidgets: [] }, { title: 'my-layout-2' } ];
      storage.add(newLayouts);
      expect(newLayouts[0].dashboard.defaultWidgets).toEqual([]);
      expect(newLayouts[1].dashboard.defaultWidgets).toEqual(options.defaultWidgets);
    });

  });

  describe('the remove method', function() {

    var storage, options;

    beforeEach(function() {
      options = {
        storageId: 'testingStorage',
        storageHash: 'ds5f9d1f',
        stringifyStorage: true,
        widgetDefinitions: [
          { name: 'A' },
          { name: 'B' },
          { name: 'C' }
        ],
        defaultLayouts: [
          { title: '1' },
          { title: '2', active: true },
          { title: '3' }
        ],
        widgetButtons: false,
        explicitSave: false
      }

      storage = new LayoutStorage(options);
    });

    it('should remove the supplied layout', function() {
      var layout = storage.layouts[1];
      storage.remove(layout);
      expect(storage.layouts.indexOf(layout)).toEqual(-1);
    });

    it('should delete the state', function() {
      var layout = storage.layouts[1];
      storage.setItem(layout.id, {});
      storage.remove(layout);
      expect(storage.states[layout.id]).toBeUndefined();
    });

    it('should do nothing if layout is not in layouts', function() {
      var layout = {};
      var before = storage.layouts.length;
      storage.remove(layout);
      var after = storage.layouts.length;
      expect(before).toEqual(after);
    });

    it('should set another dashboard to active if the layout removed was active', function() {
      var layout = storage.layouts[1];
      storage.remove(layout);
      expect(storage.layouts[0].active || storage.layouts[1].active).toEqual(true);
    });

    it('should set the layout at index 0 to active if the removed layout was 0', function() {
      storage.layouts[1].active = false;
      storage.layouts[0].active = true;
      storage.remove(storage.layouts[0]);
      expect(storage.layouts[0].active).toEqual(true);
    });

    it('should not change the active layout if it was not the one that got removed', function() {
      var active = storage.layouts[1];
      var layout = storage.layouts[0];
      storage.remove(layout);
      expect(active.active).toEqual(true);
    });

  });

  describe('the save method', function() {

    var options, storage;

    beforeEach(function() {
      options = {
        storageId: 'testingStorage',
        storage: {
          setItem: function(key, value) {

          },
          getItem: function(key) {

          },
          removeItem: function(key) {

          }
        },
        storageHash: 'ds5f9d1f',
        stringifyStorage: true,
        widgetDefinitions: [

        ],
        defaultLayouts: [
          {title: 'something'},
          {title: 'something'},
          {title: 'something'}
        ],
        widgetButtons: false,
        explicitSave: false
      }
      storage = new LayoutStorage(options);
    });
    
    it('should call options.storage.setItem with a stringified object', function() {
      spyOn(options.storage, 'setItem' );
      storage.save();
      expect(options.storage.setItem).toHaveBeenCalled();
      expect(options.storage.setItem.calls.argsFor(0)[0]).toEqual(storage.id);
      expect(typeof options.storage.setItem.calls.argsFor(0)[1]).toEqual('string');
      expect(function(){
        JSON.parse(options.storage.setItem.calls.argsFor(0)[1]);
      }).not.toThrow();
    });

    it('should save an object that has layouts, states, and storageHash', function() {
      spyOn(options.storage, 'setItem' );
      storage.save();
      var obj = JSON.parse(options.storage.setItem.calls.argsFor(0)[1]);
      expect(obj.hasOwnProperty('layouts')).toEqual(true);
      expect(obj.layouts instanceof Array).toEqual(true);
      expect(obj.hasOwnProperty('states')).toEqual(true);
      expect(typeof obj.states).toEqual('object');
      expect(obj.hasOwnProperty('storageHash')).toEqual(true);
      expect(typeof obj.storageHash).toEqual('string');
    });

    it('should call options.storage.setItem with an object when stringifyStorage is false', function() {
      options.stringifyStorage = false;
      storage = new LayoutStorage(options);
      spyOn(options.storage, 'setItem' );
      storage.save();
      expect(options.storage.setItem).toHaveBeenCalled();
      expect(options.storage.setItem.calls.argsFor(0)[0]).toEqual(storage.id);
      expect(typeof options.storage.setItem.calls.argsFor(0)[1]).toEqual('object');
    });

  });

  describe('the setItem method', function() {
    
    var options, storage;

    beforeEach(function() {
      options = {
        storageId: 'testingStorage',
        storage: {
          setItem: function(key, value) {

          },
          getItem: function(key) {

          },
          removeItem: function(key) {

          }
        },
        storageHash: 'ds5f9d1f',
        stringifyStorage: true,
        widgetDefinitions: [

        ],
        defaultLayouts: [
          {title: 'something'},
          {title: 'something'},
          {title: 'something'}
        ],
        widgetButtons: false,
        explicitSave: false
      }
      storage = new LayoutStorage(options);
    });

    it('should set storage.states[id] to the second argument', function() {
      var state = { some: 'thing'};
      storage.setItem('id', state);
      expect(storage.states.id).toEqual(state);
    });

    it('should call save', function() {
      spyOn(storage, 'save');
      var state = { some: 'thing'};
      storage.setItem('id', state);
      expect(storage.save).toHaveBeenCalled();
    });

  });

  describe('the getItem method', function() {

    var options, storage;

    beforeEach(function() {
      options = {
        storageId: 'testingStorage',
        storage: {
          setItem: function(key, value) {

          },
          getItem: function(key) {

          },
          removeItem: function(key) {

          }
        },
        storageHash: 'ds5f9d1f',
        stringifyStorage: true,
        widgetDefinitions: [

        ],
        defaultLayouts: [
          {title: 'something'},
          {title: 'something'},
          {title: 'something'}
        ],
        widgetButtons: false,
        explicitSave: false
      }
      storage = new LayoutStorage(options);
    });
    
    it('should return states[id]', function() {
      storage.states['myId'] = {};
      var result = storage.getItem('myId');
      expect(result === storage.states['myId']).toEqual(true);
    });

  });

  describe('the getActiveLayout method', function() {
    var options, storage;

    beforeEach(function() {
      options = {
        storageId: 'testingStorage',
        storage: {
          setItem: function(key, value) {

          },
          getItem: function(key) {

          },
          removeItem: function(key) {

          }
        },
        storageHash: 'ds5f9d1f',
        stringifyStorage: true,
        widgetDefinitions: [

        ],
        defaultLayouts: [
          {title: 'i am active', active: true},
          {title: 'i am not'},
          {title: 'me neither'}
        ],
        widgetButtons: false,
        explicitSave: false
      }
      storage = new LayoutStorage(options);
    });

    it('should return the layout with active:true', function() {
      var layout = storage.getActiveLayout();
      expect(layout.title).toEqual('i am active');
    });

    it('should return false if no layout is active', function() {
      var layout = storage.getActiveLayout();
      layout.active = false;
      var result = storage.getActiveLayout();
      expect(result).toEqual(false);
    });

  });

  describe('the removeItem', function() {
    
    var options, storage;

    beforeEach(function() {
      options = {
        storageId: 'testingStorage',
        storage: {
          setItem: function(key, value) {

          },
          getItem: function(key) {

          },
          removeItem: function(key) {

          }
        },
        storageHash: 'ds5f9d1f',
        stringifyStorage: true,
        widgetDefinitions: [

        ],
        defaultLayouts: [
          {title: 'i am active', active: true},
          {title: 'i am not'},
          {title: 'me neither'}
        ],
        widgetButtons: false,
        explicitSave: false
      }
      storage = new LayoutStorage(options);
    });

    it('should remove states[id]', function() {
      var state = {};
      storage.setItem('1', state);
      storage.removeItem('1');
      expect(storage.states['1']).toBeUndefined();
    });

    it('should call save', function() {
      spyOn(storage, 'save');
      var state = {};
      storage.setItem('1', state);
      storage.removeItem('1');
      expect(storage.save).toHaveBeenCalled();
    });

  });

});