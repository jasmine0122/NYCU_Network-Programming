#! /usr/bin/env python3
import os

N_SERVERS = 5

FORM_METHOD = 'GET'
FORM_ACTION = 'hw4.cgi'

TEST_CASE_DIR = 'test_case'
try:
    test_cases = sorted(os.listdir(TEST_CASE_DIR))
except:
    test_cases = []
test_case_menu = ''.join([f'<option value="{test_case}">{test_case}</option>' for test_case in test_cases])

DOMAIN = 'cs.nctu.edu.tw'
hosts = [f'nplinux{i + 1}' for i in range(5)]
host_menu = ''.join([f'<option value="{host}.{DOMAIN}">{host}</option>' for host in hosts])

print('Content-type: text/html', end='\r\n\r\n')

print('''
<!DOCTYPE html>
<html lang="en">
  <head>
    <title>NP Project 4 Panel</title>
    <link
      rel="stylesheet"
      href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css"
      integrity="sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO"
      crossorigin="anonymous"
    />
    <link
      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
      rel="stylesheet"
    />
    <link
      rel="icon"
      type="image/png"
      href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png"
    />
    <style>
      * {
        font-family: 'Source Code Pro', monospace;
      }
    </style>
  </head>
  <body class="bg-secondary pt-5">''', end='')

print(f'''
    <form action="{FORM_ACTION}" method="{FORM_METHOD}">
      <table class="table mx-auto bg-light" style="width: inherit">
        <thead class="thead-dark">
          <tr>
            <th scope="col">#</th>
            <th scope="col">Host</th>
            <th scope="col">Port</th>
            <th scope="col">Input File</th>
          </tr>
        </thead>
        <tbody>''', end='')

for i in range(N_SERVERS):
    print(f'''
          <tr>
            <th scope="row" class="align-middle">Session {i + 1}</th>
            <td>
              <div class="input-group">
                <select name="h{i}" class="custom-select">
                  <option></option>{host_menu}
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="p{i}" type="text" class="form-control" size="5" />
            </td>
            <td>
              <select name="f{i}" class="custom-select">
                <option></option>
                {test_case_menu}
              </select>
            </td>
          </tr>''', end='')

print(f'''
        <tr>
            <th scope="row" class="align-middle">Socks Server</th>
            <td>
              <div class="input-group">
                <select name="sh" class="custom-select">
                  <option></option>{host_menu}
                </select>
                <div class="input-group-append">
                  <span class="input-group-text">.cs.nctu.edu.tw</span>
                </div>
              </div>
            </td>
            <td>
              <input name="sp" type="text" class="form-control" size="5" />
            </td>
            <td></td>
        </tr>
      ''', end='')

print('''
          <tr>
            <td colspan="3"></td>
            <td>
              <button type="submit" class="btn btn-info btn-block">Run</button>
            </td>
          </tr>
        </tbody>
      </table>
    </form>
  </body>
</html>''', end='')
