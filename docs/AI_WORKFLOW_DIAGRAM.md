flowchart TD
    A[Human Intent\nGoal, Constraints, Scope] --> B[ChatGPT\nArchitecture & Design]
    B --> C[Handoff Package\nHANDOFF_TEMPLATE.md]
    C -->|Required| D[Cursor AI\nImplementation Agent]

    D --> E[Code Changes\nScoped & Mechanical]

    E --> F{Build Passes?}
    F -->|No| G[Stop\nReturn to ChatGPT]
    G --> B

    F -->|Yes| H[Human Review]
    H --> I{SOP & Handoff\nCompliant?}

    I -->|No| G
    I -->|Yes| J[Pull Request]

    J --> K[PR Checklist\n+ AI Disclosure]
    K --> L{Approved?}

    L -->|No| G
    L -->|Yes| M[Merge]

    M --> N[Freeze & Document\nIf Interfaces Stabilized]