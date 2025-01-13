# HTTP Client Implementation
Author: Dan Shani

Description:
This is a simple HTTP client implementation in C,
that supports GET requests with parameters and handles absolute and relative HTTP redirects.
The client can connect to web servers, send HTTP requests, and process responses.

Features:
- HTTP GET request support
- Parameter handling with -r flag
- HTTP redirect (3xx) handling
- Custom port specification in URLs
- error handling
- Buffer management for large responses

Usage:
client [-r n <pr1=value1 pr2=value2 ...>] <URL>

Arguments:
-r n: Specify n parameters to be added to the request
pr1=value1: Parameter name-value pairs
URL: Target HTTP URL (must start with "http://")

Example Usage:
./client http://example.com
./client -r 2 name=john age=25 http://example.com
./client http://example.com:8080/path

Technical Details:
The system incorporates socket-based network communication, dynamic memory management for handling responses, and robust URL parsing and validation. It also supports HTTP header construction and manages redirects efficiently using a goto-based implementation.

Build Requirements:
- C compiler (gcc recommended)
- POSIX-compliant system
- Standard C libraries
- Network libraries (arpa/inet.h, netdb.h)

Limitations:
- Only supports HTTP (not HTTPS)
- Only implements GET requests

Error Handling:
The program features comprehensive error handling,
addressing issues such as invalid command-line arguments, memory allocation failures,
network connection problems, invalid URL formats, and incorrect port numbers.

Notes:
Debug mode can be enabled by setting DEBUG_MODE to 1 in the source code.
This will enable additional features like saving received image files.
