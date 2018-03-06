# Project Discription
-Client send two set of packages, first set send 5 packages with one successful, and 4 error packages. The error packages are OUT_OF_SEQUENCE, LENGTH MISMATCH, PACKET_END_MISSING, and DUPLICATE_PACKET. The second set mimic the phone connection request, the client send the package with access permission request, the server send response back based on whether the client number has paid, or matched technology, or the number exist in the database. Client send 4 access packages with only one has ACCESS_OK code, and others with SEND_NO_PAID, NUM_NOT_FIND, and TECH_NOT_FOUND response.

## File Description
- Client.c: send packages
- Server.c: receive packages and send back ACK
- Customized_UDP: design the customized UDP protocol for the package

## Demo Screenshots
Copyright 2017 Stella Li
