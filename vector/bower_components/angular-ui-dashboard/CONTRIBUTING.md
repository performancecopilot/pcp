Contributing Guidelines
=======================
This project welcomes new contributors.

Licensing
---------
You acknowledge that your submissions to DataTorrent on this repository are made pursuant the terms of the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0.html) and constitute "Contributions," as defined therein, and you represent and warrant that you have the right and authority to do so.

When adding **new javascript files**, please include the following Apache v2.0 license header at the top of the file, with the fields enclosed by brackets "[]" replaced with your own identifying information. **(Don't include the brackets!)**:

```JavaScript
/*
 * Copyright (c) [XXXX] [NAME OF COPYRIGHT OWNER]
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
```

Development
-----------
To start development:

1. Fork this repository and clone the fork onto your development machine:
    ```
    $ git clone git://github.com/[USERNAME]/malhar-angular-dashboard.git
    ```

2. Install dependencies
    ```
    $ npm install .
    $ bower install 
    ```

3. Create a branch for your new feature or bug fix
    ```
    $ git checkout -b my_feature_branch
    ```

4. Run grunt tasks
    When you have finished your bug fix or feature, be sure to always run `grunt`, which will run all necessary tasks, including code linting, testing, and generating distribution files. 
    ```
    $ grunt
    ```

5. Commit changes, push to your fork
    ```
    $ git add [CHANGED FILES]
    $ git commit -m 'descriptive commit message'
    $ git push origin [BRANCH_NAME]
    ```

6. Issue a pull request
    Using the Github website, issue a pull request. We will review it and either merge your changes or explain why we didn't.

