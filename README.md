# c-web-modules: Modules for the Web  

> **Note:**  
> This project is currently a **proof of concept** and represents the **bare minimum viable product (MVP)**.  
> It is not designed for production use, and there may be bugs, limitations, or incomplete features.  
> Use at your own discretion.

Welcome to **c-web-modules**, a modular and efficient approach to web development in C. Inspired by kernel modules and AWS Lambda, this project allows you to upload C code directly to the server, which compiles and deploys it at runtime. No precompilation is necessary, and the server can easily be upgraded to include more features or external libraries.

## Addressing the Challenges of C for Web Development  

C isn’t typically the go-to for web development, and there are valid reasons why. Here’s how **c-web-modules** tackles some of the common concerns:  

1. **Slow Build Cycles**:  
   Instead of recompiling the entire application, **c-web-modules** allows you to upload raw C code. The server compiles it on-the-fly, enabling rapid iteration and eliminating the need for restarts. This is as close to "hot reloading" as you can get with C.  

2. **Speed vs. Practicality**:  
   While raw computation speed might not always be critical for web apps, **c-web-modules** shines in scenarios where performance matters, like handling heavy data processing or real-time applications. Modules let you inject optimized performance-critical code where it’s needed.  

3. **Manpower and Time-to-Market**:  
   By automating common server tasks (e.g., routing, module integration, and shared resources like SQLite3), **c-web-modules** reduces boilerplate and accelerates development compared to starting from scratch. It's not as fast as scripting languages, but it's far from the manual grind of traditional C projects.  

4. **Memory Management and Crashes**:  
   Modules are isolated and dynamically managed, reducing the risk of crashing the entire server. While C still requires careful memory management, the modular approach lets you focus on smaller, manageable pieces of functionality rather than tackling a monolithic application.  

5. **Pre-Made Solutions**:  
   By supporting external libraries like SQLite3, OpenSSL, and Jansson out of the box, **c-web-modules** leverages existing solutions, allowing developers to skip reinventing the wheel and focus on their application's unique needs.  

This isn’t a silver bullet—it’s a proof of concept. But **c-web-modules** aims to bring C’s raw power into the web world in a more developer-friendly way.  

---

## Example: Counter Module  

Here’s a simple example of a module that keeps track of a counter and returns its value every time you visit `/counter`.
See more examples in the `example/` folder.

[Websocket](example/websocket.c)

[Chat application](example/chat.c)

[JSON](example/json.c)

[TODO App](example/todo.c)

SQLite3 App (TODO)

#### `counter.c`  
```c
#include <stdio.h>
#include <cweb.h>

static int counter = 0;
static const char* template = 
    "<html>\n"
    "  <body>\n"
    "    <h1>Counter: %d</h1>\n"
    "  </body>\n"
    "</html>\n";

/* Route: /counter - Method GET */
static int index_route(struct http_request *req, struct http_response *res) {
    snprintf(res->body, HTTP_RESPONSE_SIZE, template, counter++);
    res->status = HTTP_200_OK;
    return 0;
}

/* Define the routes for the module */
export module_t config = {
    .name = "counter",
    .author = "cweb",
    .routes = {
        {"/counter", "GET", index_route, NONE},
    },
    .size = 1,
};
```

---

## Why Use c-web-modules?  

1. **Code Deployment**: Upload raw C code to the server for on-the-fly compilation and deployment.  
2. **No Precompilation**: Simplify your workflow—focus on writing code, and let the server handle compilation.  
3. **Dynamic Updates**: Add or replace functionality without downtime or recompiling the entire server.  
4. **Performance**: Written in C, the server offers unmatched speed and efficiency.  
5. **WebSocket Support**: Even when modules are updated, existing WebSocket connections remain alive.  
6. **Built-In Features**: Includes a cross-module cache and scheduler for deferred tasks.
7. **Regex in Paths**: Define routes using regular expressions for more flexible and powerful URL matching. 

Currently supported external libraries:  
- **OpenSSL**: Currently only for hashing, but later for secure communication.  
- **SQLite3**: Shared by all modules for lightweight database needs.  
- **Jansson**: For easy JSON parsing and manipulation.  

---

# Deployment  

Deploying code to the server is simple and can be done in multiple ways, depending on your workflow.


### 1. Basic Deployment with `curl`  

At its core, deploying code to the server involves sending a POST request with the C file attached. Here’s an example using `curl`:  

`curl -X POST -F "code=@path/to/yourcode.c" http://localhost:8080/mgnt`

### 2. Using the cweb script and .ini config
The script handles:  
- Sending the file to the server using `curl`.  
- Parsing responses for success or failure.  
- Providing helpful logs and error messages.  

#### Deploying Multiple Files with a Config File  
`./cweb deploy path/to/yourcode.c`

You can deploy multiple modules in one go using a configuration file. By default, the script looks for a file named `routes.ini`.  

Example `routes.ini` file:  
```ini
server_url=http://localhost:8080/mgnt

[modules]
example1.c
example2.c
```

When using the .ini files you run: `./cweb deploy`

### Errors

Error messages are forwarded back to you over http.

---

# Build it yourself!

> **Note:**  
>  MacOS support is not guaranteed!

The project depends on:

```bash
# Linux
sudo apt-get install libssl-dev
sudo apt-get install libsqlite3-dev
sudo apt-get install libjansson-dev

# MacOS
brew install openssl@3
brew install sqlite
brew install jansson
```

Run make to compile and make run to start the server.

```bash
make
make run
```

## Docker

```bash
docker-compose up --build
```

# How is c-web-modules different from Apache Modules and ISAPI Extensions?

Unlike Apache modules and ISAPI extensions, which are tightly integrated into the server and require configuration changes followed by a server restart or reload, **c-web-modules** offers runtime flexibility.

**Key Differences:**
- **Dynamic Deployment**:  
  c-web-modules allows you to upload raw C code directly to the server, which compiles and integrates it into the running application without restarts. Neither Apache modules nor ISAPI extensions support this level of runtime modification.

- **Module Isolation**:  
  Each module in c-web-modules is independently managed, minimizing the risk of crashing the entire server.

- **WebSocket Updates**:  
  WebSocket handlers can be updated at runtime without breaking existing connections, a feature not typically available in Apache modules or ISAPI.

This makes **c-web-modules** suitable for rapid experimentation and modular design, especially in scenarios requiring frequent updates without disrupting service.
