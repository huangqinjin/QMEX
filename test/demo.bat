@setlocal
@set "cli=%~1"
@set "file=%~2"
@if [%cli%] == [] @set "cli=qmex-cli.exe"
@if [%file%] == [] @set "file=%~dpn0.ini"
@findstr /v "^[@;#]" "%~dpnx0" | %cli% %file%
@set ec=%errorlevel%
@if [%~1] == [] @pause
@exit /b %ec%


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;Queries;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

# Query [1]
Grade:2 Subject:Math Score:80   Class Average Weight

# Query [2] (Query [1] With Validation)
Grade:2 Subject:Math Score:80   Class:B Average:80 Weight:0.894

# Query [3]
Grade:1 Subject:Math Score:99   Weight

# Query [4] (Query [3] With Validation)
Grade:1 Subject:Math Score:99   Weight:0.6

# Query [5]
Grade:2 Subject:Art Score:100   Class:A Average:95 Weight

