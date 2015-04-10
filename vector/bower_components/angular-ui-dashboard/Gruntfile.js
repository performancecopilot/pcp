'use strict';

module.exports = function (grunt) {
  require('load-grunt-tasks')(grunt);
  require('time-grunt')(grunt);

  grunt.initConfig({
    ngtemplates: {
      dashboard: {
        options: {
          module: 'ui.dashboard'
        },
        src: ['template/*.html'],
        dest: 'template/dashboard.js'
      }
    },
    karma: {
      unit: {
        configFile: 'karma.conf.js',
        singleRun: true
      },
      auto: {
        configFile: 'karma.conf.js'
      }
    },
    concat: {
      dist: {
        src: [
          'src/directives/dashboard.js',
          'src/directives/*.js',
          'src/models/*.js',
          'src/controllers/*.js',
          'template/dashboard.js'
        ],
        dest: 'dist/angular-ui-dashboard.js'
      }
    },
    watch: {
      files: [
        'src/**/*.*',
        'template/*.html'
      ],
      tasks: ['ngtemplates', 'concat', 'copy:dist'],
      livereload: {
        options: {
          livereload: '<%= connect.options.livereload %>'
        },
        files: [
          'demo/{,*/}*.html',
          'demo/{,*/}*.css',
          'demo/{,*/}*.js',
          'dist/*.css',
          'dist/*.js'
        ]
      }
    },
    jshint: {
      options: {
        jshintrc: '.jshintrc'
      },
      all: [
        'Gruntfile.js',
        'src/{,*/}*.js'
      ]
    },
    copy: {
      dist: {
        files: [{
          expand: true,
          flatten: true,
          src: ['src/angular-ui-dashboard.css'],
          dest: 'dist'
        }]
      }
    },
    clean: {
      dist: {
        files: [{
          src: [
            'dist/*'
          ]
        }]
      },
      templates: {
        src: ['<%= ngtemplates.dashboard.dest %>']
      }
    },
    connect: {
      options: {
        port:9000,
        // Change this to '0.0.0.0' to access the server from outside.
        hostname: 'localhost',
        livereload: 35729
      },
      livereload: {
        options: {
          open: true,
          base: [
            '.',
            'demo',
            'dist'
          ]
        }
      }
    }
  });

  grunt.registerTask('test', [
    'jshint',
    'ngtemplates',
    'karma:unit'
  ]);

  grunt.registerTask('test_auto', [
    'jshint',
    'ngtemplates',
    'karma:auto'
  ]);

  grunt.registerTask('demo', [
    'connect:livereload',
    'watch'
  ]);

  grunt.registerTask('default', [
    'clean:dist',
    'jshint',
    'ngtemplates',
    'karma:unit',
    'concat',
    'copy:dist',
    'clean:templates'
  ]);
};
