.. _AboutThisGuide:

About Programmer's Guide
#########################

This guide describes how to program the Performance Co-Pilot (PCP) performance analysis toolkit. PCP provides a systems-level suite of tools that cooperate to 
deliver distributed performance monitoring and performance management services spanning hardware platforms, operating systems, service layers, database internals, 
user applications and distributed architectures.

PCP is an open source, cross-platform software package - customizations, extensions, source code inspection, and tinkering in general is actively encouraged.

“About Programmer's Guide” includes short descriptions of the chapters in this book, directs you to additional sources of information, and explains typographical conventions.

.. contents::

⁠What This Guide Contains
**************************

This guide contains the following chapters:

Chapter 1, :ref:`Programming Performance Co-Pilot`, contains a thumbnail sketch of how to program the various PCP components.

Chapter 2, :ref:`Writing a PMDA`, describes how to write Performance Metrics Domain Agents (PMDAs) for PCP.

Chapter 3, :ref:`PMAPI--The Performance Metrics API`, describes the interface that allows you to design custom performance monitoring tools.

Chapter 4, :ref:`Instrumenting Applications`, introduces techniques, tools and interfaces to assist with exporting performance data from within applications.

Audience for This Guide
************************

The guide describes the programming interfaces to Performance Co-Pilot (PCP) for the following intended audience:

* Performance analysts or system administrators who want to extend or customize performance monitoring tools available with PCP

* Developers who wish to integrate performance data from within their applications into the PCP framework

This book is written for those who are competent with the C programming language, the UNIX or the Linux operating systems, and the target domain from which the 
desired performance metrics are to be extracted. Familiarity with the PCP tool suite is assumed.

Related Resources
******************

The *Performance Co-Pilot User's and Administrator's Guide* is a companion document to the *Performance Co-Pilot Programmer's Guide*, and is intended for system 
administrators and performance analysts who are directly using and administering PCP installations.

The *Performance Co-Pilot Tutorials and Case Studies* provides a series of real-world examples of using various PCP tools, and lessons learned from deploying the 
toolkit in production environments. It serves to provide reinforcement of the general concepts discussed in the other two books with additional case studies, and 
in some cases very detailed discussion of specifics of individual tools.

Additional resources include man pages and the project web site.

Man Pages
**********

The operating system man pages provide concise reference information on the use of commands, subroutines, and system resources. There is usually a man page for 
each PCP command or subroutine. To see a list of all the PCP man pages, start from the following command::

 man PCPIntro

Each man page usually has a "SEE ALSO" section, linking to other, related entries.

To see a particular man page, supply its name to the **man** command, for example::

 man pcp
 
The man pages are arranged in different sections separating commands, programming interfaces, and so on. For a complete list of manual sections on a platform enter 
the command::

 man man

When referring to man pages, this guide follows a standard convention: the section number in parentheses follows the item. For example, **pminfo(1)** refers to the 
man page in section 1 for the **pminfo** command.

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