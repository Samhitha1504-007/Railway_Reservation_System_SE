# Railway Reservation System (RRS)

A C/C++ Railway Reservation System featuring an interactive ncurses TUI, graph-based route search, dynamic fare optimization, and automated waitlist management. Developed by Team 5 for the Software Engineering Mini-Project.

## Core Features

*   Passenger registration, secure login, and role-based session management.
*   Graph-based route search across direct and multi-leg (connected) journeys.
*   Filtering and sorting of train options by fare, travel time, and eco-routing metrics.
*   Ticket booking with class selection and unique PNR generation.
*   Automatic waitlist queueing when a class/train is at capacity.
*   Ticket cancellation with automatic waitlist promotion to free seats.
*   Administrative management of trains, routes, station halts, and seat inventory.
*   Administrative operational reporting detailing bookings and revenue.

## Architecture & Technology Stack

*   **Core Engine:** C/C++.
*   **User Interface:** Text User Interface (TUI) built with the ncurses library (with an optional decoupled MERN/API wrapper).
*   **Persistence:** File storage or a lightweight embedded database.
*   **Version Control & Agile Planning:** Git, GitHub, and GitHub Projects / Jira.
*   **Build & CI/CD Pipeline:** Make/CMake and GitHub Actions / Jenkins.
*   **Testing & Static Analysis:** gtest (C++) / Unity or Check (C), and SonarQube / cppcheck.

## Team Profile & Module Ownership

The system's core business logic is modularized, ensuring independent development and testing without overlapping dependencies. 

*   **Ridhima Jain (PES1UG24CS373):** User Access & Profile Module. Handles onboarding, secure credential verification, and Role-Based Access Control (RBAC).
*   **S S Adhithya Sriram (PES1UG24CS393):** Inventory & Schedule Module (Admin). Manages the definition of train numbers, multi-station routes, departure/arrival times, station halts, and per-class seat capacities.
*   **Samhitha S (PES1UG24CS411):** Search Engine, Fare & Eco-Optimization Module. Implements the graph-based route search, computes dynamic fares based on distance and class, and calculates eco-routing metrics.
*   **Saathvik Shenoy Padubidri (PES1UG24CS394):** Booking & Queue Management Module. Implements transactional booking logic, PNR generation, structured waitlist queueing, and automatic waitlist promotion during cancellations.

## Project Documentation

This repository houses the complete documentation for the Software Development Life Cycle (SDLC) of the RRS project. 

*   **`01_SYNOPSIS/`**: Contains the approved project proposal detailing the problem statement, scope, and planned sprint schedule.
*   **`02_REQUIREMENTS/`**: Contains the Software Requirements Specification (SRS v1.0), detailing functional, non-functional, and security requirements alongside the initial validation planning.

## Getting Started

*(To be populated as development progresses during Sprint 1 & 2)*

1.  Clone the repository: `git clone <repository_url>`
2.  Navigate to the project directory.
3.  Ensure you have `ncurses`, `make`, and a standard C/C++ compiler (e.g., GCC/Clang) installed on your Linux environment.
4.  Run `make build` to compile the core engine and TUI.
5.  Run `./rrs_app` to launch the terminal interface.
