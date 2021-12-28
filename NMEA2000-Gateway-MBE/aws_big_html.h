const char AWS_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>

<meta HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<style>
h1 {
  font-size: 2em;
  text-align: center; 
  vertical-align: middle; 
  margin:0 auto;
}

p {
  font-size: 1.5em;
  text-align: center; 
  vertical-align: middle; 
  margin:0 auto;
}

table {
  font-size: 3em;
  text-align: left; 
  vertical-align: middle; 
  margin:0 auto;
}

.button {
  font-size: 22px;;
  text-align: center; 
}

.slidecontainer {
  width: 100%;
}

.slider {
  -webkit-appearance: none;
  width: 80%;
  height: 22px;
  background: #d3d3d3;
  outline: none;
  opacity: 0.7;
  -webkit-transition: .2s;
  transition: opacity .2s;
}

.slider:hover {
  opacity: 1;
}

.slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  appearance: none;
  width: 18px;
  height: 18px;
  background: #4CAF50;
  cursor: pointer;
}

.slider::-moz-range-thumb {
  width: 18px;
  height: 18px;
  background: #4CAF50;
  cursor: pointer;
}

</style>

<title>AWS Display</title>
<hr>
<h1>Wind</h1>
</head>

<body style="font-family: verdana,sans-serif" BGCOLOR="#819FF7">

  <hr>
  <table>
    <tr><td style="text-align:right;">AWS:</td><td style="color:white;"><span id='aws'></span> kn</td></tr>
    <tr><td style="text-align:right;">MaxAWS:</td><td style="color:white;"><span id='maxaws'></span> kn</td></tr>    
  </table>
  <hr>

  <script>
 
    requestData(); // get intial data straight away 

    // request data updates every 500 milliseconds
    setInterval(requestData, 500);
    
    function requestData() {

      var xhr = new XMLHttpRequest();
      
      xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {

          if (this.responseText) { // if the returned data is not null, update the values

            var data = JSON.parse(this.responseText);

            document.getElementById("aws").innerText = data.aws;
            document.getElementById("maxaws").innerText = data.maxaws;
                        
          } 
        } 
      }
      xhr.open('GET', 'get_data', true);
      xhr.send();
    }
     
  </script>

</body>

</html>

)=====";
