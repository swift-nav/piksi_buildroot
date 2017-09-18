# Network diagram for translating NTRIP RTCM to SBP
```
┌───────────────────┐                                             
│                   │     HTTP Request      ┌────────────────────┐
│        Piksi      │         (TCP)         │   Someone else's   │
│    NTRIP Daemon   │──────────────────────▶│    NTRIP Server    │
│                   │                       └────────────────────┘
└───────────────────┘                                             
          │                                                       
          │                                                       
          └─────┐                                                 
                │  FIFO output                                    
                ▼ (RTCM packets)    ┌──────────────────┐          
┌───────────────────┐               │    ZMQ Router    │          
│                   │  ┌───────────▶│  (Routes 45031   │──┐       
│     ZMQ Adapter   │  │            │    to 45010)     │  │       
│   (+ RTCM framer) │  │            └──────────────────┘  │       
│                   │  │                                  │       
└─────────┬─────────┘  │          ┌───────────────────────┘       
          │            │          │            ZMQ Pub to 45010   
    ZMQ Pub to 45031   │          │                               
          │────────────┘          │  ┌────────────────────────┐   
          │           ▲           │  │                        │   
          ▼           │           │  │      rtcm3_bridge      │   
                      │           │  │                        │   
                      │           └─▶│(Bridge from RTCM to SBP│   
                      │              │that provides correction│   
             ┌─────────────────┐     │         data)          │   
             │  Other things   │     │                        │   
             │ (like TCP port  │     └────────────────────────┘   
             │  adpaters) can  │                                  
             │ setup a ZMQ pub │                                  
             │    to 45031     │                                  
             └─────────────────┘                                  
```
