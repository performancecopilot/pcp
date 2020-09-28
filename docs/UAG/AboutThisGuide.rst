.. _AboutThisGuide:

About User's and Administrator's Guide
#######################################

This guide describes the Performance Co-Pilot (PCP) performance analysis toolkit. PCP provides a systems-level suite of tools that cooperate to deliver 
distributed performance monitoring and performance management services spanning hardware platforms, operating systems, service layers, database internals, 
user applications and distributed architectures.

PCP is a cross-platform, open source software package - customizations, extensions, source code inspection, and tinkering in general is actively encouraged.

"About This Guide" includes short descriptions of the chapters in this book, directs you to additional sources of information, and explains typographical conventions.

.. contents::

⁠What This Guide Contains
*************************

This guide contains the following chapters:

Chapter 1, :ref:`IntroductionToPCP`, provides an introduction, a brief overview of the software components, and conceptual foundations of the PCP software.

Chapter 2, :ref:`InstallingAndConfiguringPcp`, describes the basic installation and configuration steps necessary to get PCP running on your systems.

Chapter 3, :ref:`CommonConventionsAndArguments`, describes the user interface components that are common to most of the text-based utilities that make up the monitor portion of PCP.

Chapter 4, :ref:`MonitoringSystemPerformance`, describes the performance monitoring tools available in Performance Co-Pilot (PCP).

Chapter 5, :ref:`PerformanceMetricsInferenceEngine`, describes the Performance Metrics Inference Engine (pmie) tool that provides automated monitoring of, and reasoning about, system performance within the PCP framework.

Chapter 6, :ref:`ArchiveLogging`, covers the PCP services and utilities that support archive logging for capturing accurate historical performance records.

Chapter 7, :ref:`Performance Co-Pilot Deployment Strategies`, presents the various options for deploying PCP functionality across cooperating systems.

Chapter 8, :ref:`Customizing and Extending PCP Services`, describes the procedures necessary to ensure that the PCP configuration is customized in ways that maximize the coverage and quality of performance monitoring and management services.

Appendix A, Acronyms, provides a comprehensive list of the acronyms used in this guide and in the man pages for Performance Co-Pilot.

Audience for This Guide
************************

This guide is written for the system administrator or performance analyst who is directly using and administering PCP applications.

Related Resources
******************

The *Performance Co-Pilot Programmer's Guide*, a companion document to the *Performance Co-Pilot User's and Administrator's Guide*, is intended for 
developers who want to use the PCP framework and services for exporting additional collections of performance metrics, or for delivering new or customized 
applications to enhance performance management.

The *Performance Co-Pilot Tutorials and Case Studies* provides a series of real-world examples of using various PCP tools, and lessons learned from 
deploying the toolkit in production environments. It serves to provide reinforcement of the general concepts discussed in the other two books with 
additional case studies, and in some cases very detailed discussion of specifics of individual tools.

Additional resources include man pages and the project web site.

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
     - Parentheses that follow function names surround function arguments or are empty if the function has no arguments; parentheses that follow commands surround man page section numbers.                                                                                                                 |


Reader Comments
****************

If you have comments about the technical accuracy, content, or organization of this document, contact the PCP maintainers using either the email address or the web site listed earlier.

We value your comments and will respond to them promptly.
