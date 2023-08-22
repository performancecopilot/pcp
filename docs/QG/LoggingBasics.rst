.. _LoggingBasics:

Managing Archive
################################################

.. contents::

Overview
*********

The Performance Co-Pilot includes many facilities for creating and replaying archives that capture performance information.  

For all PCP monitoring tools, metrics values may come from a real-time feed (i.e. from *pmcd* on some host), or from an archive.  

Users have complete control of what metrics are collected, how often and in which logs.  These decisions can be changed dynamically.  

Primary Logger
****************

1. The primary instance of the PCP archive logger (*pmlogger*) may be started on a 
   collector system each time *pmcd* is started. To turn it on, as **root** do the following::

    # chkconfig pmlogger on
    # $PCP_RC_DIR/pcp start

2. The specification for hosts to log and *pmlogger* options is given in the 
   ``$PCP_PMLOGGERCONTROL_PATH`` file and (optionally) files in the ``$PCP_PMLOGGERCONTROL_PATH.d`` directory.

3. This has a default entry for the local host, which specifies the metrics to be logged, and the frequency of 
   logging - by default, using the file ``$PCP_VAR_DIR/config/pmlogger/config.default``.

Other Logger Instances
************************

1. Additional instances of *pmlogger* may be started at any time, running on either a collector system or a monitor system.

2. In all cases, each *pmlogger* instance will create an archive for metrics from a single collector system.

3. The initial specification of what to log is given in a configuration file; the syntax is fully described in the *pmlogger* (1) man page.

Creating and Replaying PCP Archives
*****************************************

1. Some configuration files are supplied, which may be found in the directory ``$PCP_VAR_DIR/config/pmlogger``.

2. To create an archive and subsequent playback. Open a command shell and enter:: 
    
    $ source /etc/pcp.conf
    $ cd /tmp
    $ rm -f myarchive.*

3. To start *pmlogger* with **localhost** as the default host from which metrics will be fetched, a logging duration of **30 seconds**, a logging interval of **1 second**, 
   a configuration from the ``$PCP_VAR_DIR/config/pmlogger/config.mpvis`` file, and *myarchive* as the base name of the output archive::   
    
    $ $PCP_BINADM_DIR/pmlogger -T 30sec -t 1sec \ -c $PCP_VAR_DIR/config/pmlogger/config.mpvis myarchive

4. Once the archive has been created, the directory listing of *myarchive.\** shows that the archive created by *pmlogger* is composed of 3 files::  
    
    $ ls myarchive.*

5. *mpvis* is used for playbacks and all of the PCP monitoring tools accept the command line arguments **-a** *archivename* to replay from an archive::

    $ mpvis -a myarchive

6. When mpvis starts up, in the PCP Archive Time Control dialog

    - Single click on the Play button will start replaying values from the archive at the same speed at which they were recorded.
    - Double click on thr play button will replay the values in a Fast Forward mode until it reaches the end of the archive.
    - Select the Options->Show Bounds option to see the bounds of the archive.  
    

    Another way to look at the bounds of the archive is by using *pmdumplog*::  
    
    $ pmdumplog -L myarchive


Archive Folios
***************

1. To create an archive folio, open a command shell and enter::  

    $ source /etc/pcp.conf
    $ cd /tmp
    $ tar xzf $PCP_DEMOS_DIR/tutorials/pmie.tar.gz
    $ tar xzf $PCP_DEMOS_DIR/tutorials/cpuperf.tar.gz
    $ $PCP_BINADM_DIR/mkaf pmie/babylon.perdisk.0 \cpuperf/babylon.percpu.0 > myfolio

*mkaf* creates a folio called **myfolio** which includes the archives *babylon.percpu* and *babylon.perdisk*. 
Note that the archives are not changed in any way, and a new archive is not created.

2. *pmafm* tool may now be used to perform different operations on the folio **myfolio**, such as listing the folio contents, or using other tools to open the archives contained in the folio. If *pmafm* is given a folio name but no arguments, it will run in interactive mode.
    
    .. sourcecode:: none

        $ pmafm myfolio list
        $ pmafm myfolio pmdumplog -l

For more information on folios refer to the mkaf(1) and pmafm(1) man pages.

Controlling pmlogger
**********************

The *pmlc* utility may be used to interrogate any *pmlogger* instance running either locally or remotely.  Use *pmlc* to

 - Add or delete metrics or metric instances to be logged
 - Change the logging frequency for selected metrics

The line-oriented command interface to pmlc(1) is fully described in the man page.

Management of PCP Archives
***************************

PCP includes a suite of scripts and tools to automate the collection and management of archives.

Briefly, these facilities include:

- daily log rotation (pmlogger_daily(1))
- archive merging (pmlogextract(1))
- automatic restarting of failed pmlogger instances (pmlogger_check(1))
- creation of snapshots from archives (pmsnap(1))
- maintenance of archive folios for active archives (mkaf(1) and pmafm(1))
