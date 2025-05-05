# 10-webserver

## Build & Run

Program should ideally run on POSIX system

1.  **Build:**
    ```bash
    make
    ```
2.  **Run (Default Port 80):**
    ```bash
    sudo ./http_server
    ```
3.  **Run (Custom Port):**
    ```bash
    ./http_server -p 8080
    ```

## Endpoints

*   `GET /`: Serves `./static/index.html`.
*   `GET /static/<path>`: Serves file from `./static/<path>`.
*   `GET /calc/{add|mul|div}/<num1>/<num2>`: Performs calculation, returns HTML.

## Browser Testing

### localhost

![alt text](static/README/1.png)

### localhost/static/test.txt

![alt text](static/README/2.png)

### localhost/static/images/cat.png

![alt text](static/README/3.png)

### localhost/calc

![alt text](static/README/4.png)

![alt text](static/README/5.png)

![alt text](static/README/6.png)

## telnet Test

![alt text](static/README/7.png)

## Postman Test

![alt text](static/README/8.png)