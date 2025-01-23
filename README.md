This repository contains the implementation of a real-time management program for an Automated Pharmacy Storage System. The system utilizes real-time kernel approaches to ensure precise and timely management of the pharmacy's storage, handling tasks such as inventory control, stock updates, and system interactions in real-time.

Description:

The project focuses on applying real-time concepts for the management of an Automated Pharmacy Storage System, incorporating real-time kernel features to ensure efficient and responsive operations in a dynamic environment. The system is designed to manage pharmaceutical inventory with real-time processing to avoid delays and ensure proper management of stocks.

Key Features:

- Storage Constraints:
    
    The warehouse has 9 storage cells, where each cell holds a single product. Multiple instances of the same drug code/reference can be stored in different cells.
    
- Drug Storage:
    
    Each drug is stored with the following information:
    
    - Name
    - Laboratory
    - National drug code (reference)
    - Expiry date
- Drug Movement:
    - Drugs are received and delivered in position (1,1) with the cage in position `y=1`.
    - As drugs move along the `(x,z)` coordinates, the cage switches to position `y=2`.
- Expiration Management:
    - The system tracks drug expiry dates and triggers an alert (flashing lamp) whenever expired drugs exist in the storage.
    - A button press allows expired drugs to be removed from storage, with the lamp continuing to flash until the expired drug removal process is complete.
- Idle State Management:
    - When the system is idle, the drugs with the nearest expiry dates are moved to the lower empty cells for better space utilization and order.

Real-Time Approach:

The system is designed to incorporate real-time concepts with a real-time kernel, ensuring timely operations within the pharmacy storage. By leveraging the real-time kernel, the system guarantees minimal delays in drug storage management and the timely handling of operations such as:

- Inventory updates
- Expiry alerts
- Drug retrievals and storages


