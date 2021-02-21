# networks_RIP

The Routing Information Protocol (RIP) is a routing protocol based on the Bellman-Ford algorithm. RIP has been used as the intra-domain routing algoirthm in many small-scale autonomous systems on the internet since the early days of the ARPANET. This project implements a subset of the functionality of RIP Version 2 (adapted from RFC 2453).

This implementation includes the following functionality: 

Routing table maintenance and update
Split Horizon with Poisoned Reverse
Triggered Updates to speed up convergence
Timers for periodic updates and route timeout

See the Readme in the code directory for running instructions. 

Acknowledgements: This project has been adapted from Stanford's CS144 Introduction to Computer Netwoking Lab
