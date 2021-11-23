.. _AboutHowTo:

Quick Guides
#############

.. contents::

This guide will assist new users by clearly showing how to solve specific problem scenarios that users encounter frequently.
The aim of this guide is to assist first-time users to become more productive right away.

⁠What This Guide Contains
**************************

This guide contains the following chapters:

Chapter 1, :ref:`List the available performance metrics`, introduces the *pminfo* command to display various types of information about performance metrics available.

Chapter 2, :ref:`Add new metrics to the available set`, covers adding mounts metrics to *pmcd*.

Chapter 3, :ref:`Record metrics on my local system`, covers setup of logging to record metrics from local system.

Chapter 4, :ref:`Record metrics from a remote system`, covers setup of logging from remote systems.

Chapter 5, :ref:`graph a performance metric`, introduces *pmchart* - a strip chart tool for Performance Co-Pilot.

Chapter 6, :ref:`Automate performance problem detection`, introduces *pmieconf* which is used to display and modify variables or parameters controlling the details of the generated pmie rules.

Chapter 7, :ref:`Setup automated rules to write to the system log`, introduces *pmieconf* - a utility for viewing and configuring variables from generalized *pmie* (1) rules.

Chapter 8, :ref:`Record historical values for use with the pcp-dstat tool`, introduces *pcp-dstat* tool which is a general performance analysis tool to view multiple system resources instantly.

Chapter 9, :ref:`Export metric values in a comma-separated format`, introduces *pmrep* which is a customizable performance metrics reporting tool.

Chapter 10, :ref:`Using Charts`, introduces the basic functionality available in the PCP Strip Chart tool - pmchart.

Chapter 11, :ref:`Managing Archive Log`, covers PCP tools for creating and managing PCP archive logs.

Chapter 12, :ref:`Automated Reasoning with pmie` covers the pmie tool within PCP that is designed for automated filtering and reasoning about performance.

Chapter 13, :ref:`Configuring Automated Reasoning`, covers customization of pmie rules using pmieconf.

Chapter 14, :ref:`Analyzing Linux Containers`, introduces how to extract performance data from individual containers using the PCP tools.

Chapter 15, :ref:`Establishing Secure Connections`, covers setting up secure connections between PCP collector and monitor components. Also, how network connections can be made secure against eavesdropping, data tampering and man-in-the-middle class attacks.

Chapter 16, :ref:`Establishing Secure Client Connections`, covers setting up secure connections between PCP collector and monitor components and discuss setting up certificates on both the collector and monitor hosts.

Chapter 17, :ref:`Setup Authenticated Connections`, covers setting up authenticated connections between PCP collector and monitor components.

Chapter 18, :ref:`Importing data and creating PCP archives`, describes an alternative method of importing performance data into PCP by creating PCP archives from files or data streams that have no knowledge of PCP.

Chapter 19, :ref:`Using 3D views`, covers performance visualisation with pmview.

Audience for This Guide
************************

This guide is written for the system administrator or performance analyst who is directly using and administering PCP applications.

Man Pages
**********

The operating system man pages provide concise reference information on the use of commands, subroutines, and system resources. There is usually a 
man page for each PCP command or subroutine. To see a list of all the PCP man pages, start from the following command::

 man PCPIntro
 
Each man page usually has a "SEE ALSO" section, linking to other, related entries.

To see a particular man page, supply its name to the **man** command, for example::

 man pcp

The man pages are arranged in different sections - user commands, programming interfaces, and so on. For a complete list of manual sections on a platform 
enter the command::

 man man

When referring to man pages, this guide follows a standard convention: the section number in parentheses follows the item. For example, **pminfo(1)** 
refers to the man page in section 1 for the pminfo command.

Web Site
*********

The following web site is accessible to everyone:

URL : https://pcp.io

PCP is open source software released under the GNU General Public License (GPL) and GNU Lesser General Public License (LGPL)

⁠Conventions
************

The following conventions are used throughout this document:

.. list-table::
   :widths: 20 80

   * - **Convention**           
     - **Meaning**                                         
   * - ``${PCP_VARIABLE}``
     - A brace-enclosed all-capital-letters syntax indicates a variable that has been sourced from the global ``${PCP_DIR}/etc/pcp.conf`` file. These special variables indicate parameters that affect all PCP commands, and are likely to be different between platforms.
   * - **command**
     - This fixed-space font denotes literal items such as commands, files, routines, path names, signals, messages, and programming language structures. 
   * - *variable*
     - Italic typeface denotes variable entries and words or concepts being defined.                                                                      
   * - **user input**
     - This bold, fixed-space font denotes literal items that the user enters in interactive sessions. (Output is shown in nonbold, fixed-space font.)    
   * - [ ]
     - Brackets enclose optional portions of a command or directive line.                                                                                 
   * - ...
     - Ellipses indicate that a preceding element can be repeated.                                                                                        
   * - ALL CAPS
     - All capital letters denote environment variables, operator names, directives, defined constants, and macros in C programs.                         
   * - ()
     - Parentheses that follow function names surround function arguments or are empty if the function has no arguments; parentheses that follow commands surround man page section numbers.


Reader Comments
****************

If you have comments about the technical accuracy, content, or organization of this document, contact the PCP maintainers using either the email address or the web site listed earlier.

We value your comments and will respond to them promptly.
