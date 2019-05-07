---
title: Google Summer of Code Proposal Draft
path: /
---

## Contact Information
- Name - Miroslav Foltýn
- Phone - +420 732 204 141
- Email - email@miroslavfoltyn.com
- Location - Czech Republic
- Website - miroslavfoltyn.com
- Postal Address: 
    - Československé armády 338
    - Budišov nad Budišovkou 74787
    - Czech Republic
    
I also hangout on "The Programmer's hangout" Discord channel. Nick is _@Erbenos#1658_.

### Contact Information in Case of an Emergency
- Name - Kateřina Biolková
- Phone - +420 739 981 205
- Email - biolkova.katerina@gmail.com
<br><br>
- Name - Jana Foltýnová
- Phone - +420 777 106 131
- Email - foltynova@linaset.cz

# PCP PMDA Agent for StatsD in C

## Bibligraphy and Past Experience
I am Master's Computer Science student at Palacky University in Olomouc.

I have been working as a freelance web developer for 3 years, while studying simultaneously. Last 6 months or so I am part-time working at Computer Center of Palacky University, where me and my colleagues build upon and improve existing university's internal applications for employees and students alike. I mostly do front-end stuff now, altrought I am able to do back-end if need be (in either C# or PHP, I have some NodeJS experience, but only for client side app's - with stuff like Electron - not with actually coding server-side logic).
I have little low-level programming experience, only did some microcontroller programming in Assembly, C and VHDL in my secondary school years. There were some C and Assembly tasks while studying at University as well. I am looking forward to gaining more experience in C and meeting people from the community.

If my proposal get's accepted, I will fully commit myself to GSoC the moment coding begins.

### Website for Blue Partners s.r.o.
I successfully completed both front-end and back-end of given website design in collaboration with marketer (Lukáš Hladeček), copywriter (Karel Melecký), designer (Michal Maleňák) and SEO analyst (Šárka Jakubcová). The project was completed in tight deadline, as the marketing campaign was already setup and delays would be costly. Site was further improved months down the line with more functionality as requested by the client. Most important part was interactive test that aimed at inspecting company owners perception of their company's technical security and presenting gathered data in clear and informative way.

[Check it out](https://www.bluepartners.cz/en/)

### Redesign of Palacky University in Olomouc Helpdesk
In collaboration with Computer Center of Palacky University I implemented redesign of university's Helpdesk application that serves as a main place where both students and employees can submit tickets regarding various issues they may have with either their studies or jobs. I communicated closely with day-to-day users of Helpdesk about their feedback to WIP builds to accustom application's UI to their everyday needs and make switching from the older design smoother, and mainly more pleasant, experience.

[Check it out](https://helpdesk.upol.cz/)

## Abstract
**StatsD** is simple, text-based UDP protocol for receiving monitoring data of applications in architecture client-server. As of right now, there is no StatsD implementation for PCP available, other than [pcp-mmvstatsd](https://github.com/lzap/pcp-mmvstatsd) which is not suitable for production environment as it:

- does not map labels properly and creates hundreds of metrics
- doesn't use PMAPI, instead it maps memory using MMV, which was meant for instrumenting not agents

Goal of this project is to write PMDA agent for PCP in C, that would receive StatsD UDP packets and then aggregate and transfer handled data to PCP. There would be 3 basic types of metrics: counter, duration and gauge. Agent is to be build with modular architecture in mind with an option of changing implementation of both aggregator and parsers, which will allow to accurately describe differences between approaches to aggregation and text protocol parsing. Since the PMDA API is based on around callbacks the design is supposed to be multithreaded.

## Technicalities

### PMDA architecture
PMDA will be written using **pthreads**:

- Main process
    - Gets configuration options (either from file, or command-line)
    - Inicializes channels inter-thread communication entities
    - Starts threads
- Single thread collecting data from UDP/TCP
- Single thread parsing received data
- Single thread processing parsed datagrams
- Single thread communicating with PCP PMDA library, since libpcp_pmda is not thread-safe

Inter-thread communication will be taken care of using _Go_ (_Go-Lang_) like channels library **chan** ([github repo](https://github.com/tylertreat/chan), Apache license).

### Extracting metrics from the target domain
There will be 3 supported metrics:
- **counter**
- **duration**
- **gauge**

Data will come from either TCP or UDP connection in form of StatsD datagrams, which will be parsed with either:

- **custom/basic parser** - traditional procedural C code that parses a string OR
- **Ragel parser** - finite state machine represented in C code that will be generated from regular language written in domain specific language for Ragel compiler

Configuration will also include an option for specifying which parser to use, port on which to listen, max packet size and how many unprocessed datagrams there may be.

In case of **duration** metric values will be further processed via high dynamic range histogram using **HdrHistogram_c** ([github repo](https://github.com/HdrHistogram/HdrHistogram_c), BSD 2 License) or basic aggregation.

#### Ragel parser
**Ragel** ([website](http://www.colm.net/open-source/ragel/), [github repo](https://github.com/bnoordhuis/ragel)) is an Open Source MIT licensed (in case of Ragel 6, GPL v2) State Machine Compiler, as such it is great for parsing data formats.

### Extracting metric values
Values are simple strings in a form of:
```
<metricname>:<value>|<type>
```

Data may also contain any number of tags (separated by commas), which can be used as instance identifiers:
```
<metricname>,<tagname>=<tagvalue>:<value>|<type>
```
Instance will be identified by the tag of the same name e.g.:
```
<metricname>,<instance>=<identifier>:<value>|<type>
```

and/or sampling attribute:
```
<metricname>:<value>|<type>@<sampling>
```

Supported types:
- **c** = counter
- **g** = gauge
- **ms** = duration

Such strings will be parsed with configured parser and passed to another thread for further processing.

### Debugging
Standard GDB should proof sufficient for generic C code, with Valgrind specifically for memory allocations.

Most PCP tools support *-D* flag for activating debugging options. PCP also ships with **dbpmda** - domain specific debugging utility for PMDAs.

<figure>
    <img src="https://gsoc.miroslavfoltyn.com/graph.png" alt="Graph of Application">
    <figcaption style="text-align: center">
        <small>Application Architecture Graph</small>
    </figcaption>
</figure>

## Third party libraries
Libraries that are currently planned for usage in this project (excluding PCP related ones) are:
- *chan* ([github repo](https://github.com/tylertreat/chan), Apache license)
- *Ragel* ([website](http://www.colm.net/open-source/ragel/), [github repo](https://github.com/bnoordhuis/ragel), MIT or GPLv2 license, differs by version)
- *HdrHistogram_c* ([github repo](https://github.com/HdrHistogram/HdrHistogram_c), Apache license)

## Deliverables
- PMDA agent C code licensed under GNU GPL
- Code of basic and Ragel parser modules
- Code of basic and HDR histogram aggregator modules
- Example configration
- Integration tests
- Documentation in a form of READ describing compilation and basic configuration

### Configuration options
- Maximum packet size
- Maximum of unprocessed packets
- Port and address to listen to for StatsD datagrams
- Parser type
- Aggregation type
- Tracing and debugging flags

## Proposal Timeline

### Community Bonding Period
- To familiarize myself with PCP, with the help of my mentor
- To study and gain an understanding of how to write a PMDA for PCP, with help of [PCP Programmer's Guide](https://pcp.io/books/PCP_PG/pdf/pcp-programmers-guide.pdf)
- Clear up issues regarding project development setup and/or project timeline and it's milestones
- To show what is already done (as the project is also my Master's thesis that I started to work on since January 2019) 

### May 27 - June 28
- To incorporate basic aggregation
- To incorporate aggregation using HDR histogram
- To incorporate Ragel parser
- To write end-to-end/integration tests.

At this point the data parsing and processing part not related to PCP should be complete.

### June 29 - July 26
- To connect what's done with PCP
- To write end-to-end/integration tests

At this point the PMDA should fully integrate with PCP.

### July 27 - August 19
- To do a performance analysis and optimize program based on results
- To write documentation
- Bounce period for any delays

At this point documentation should be written, agent performance optimized and all programming finished so that the product is fully usable.

## Feedback
Please send feedback to my email address.