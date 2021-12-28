const char POWER_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>

<meta HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<style>
h1 {
  font-size: 1.5em;
  text-align: center; 
  vertical-align: middle; 
  margin:0 auto;
}

h2 {
  font-size: 1.0em;
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
  font-size: 1.5em;
  text-align: left; 
  vertical-align: middle; 
  margin:0 auto;
}

.button {
  font-size: 20px;;
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

<title>Power</title>
<hr>
<h1>Power</h1>
<hr>
</head>

<body style="font-family: verdana,sans-serif" BGCOLOR="#819FF7">
  <table>
    <tr><td style="text-align:right;">Boat Voltage:</td><td style="color:white;"><span id='volt'></span> V</td></tr>
    <tr><td style="text-align:right;">Fridge:</td><td style="color:white;"><span id='temp'></span> °C</td></tr>
    <tr><td> </td></tr>
    <tr><td> </td></tr>
    <tr><td style="text-align:right;">Mains Voltage:</td><td style="color:white;"><span id='voltage'></span> V</td></tr>
    <tr><td style="text-align:right;">Current:</td><td style="color:white;"><span id='current'></span> A</td></tr>
    <tr><td style="text-align:right;">Power:</td><td style="color:white;"><span id='power'></span> W</td></tr>
    <tr><td style="text-align:right;">Usage:</td><td style="color:white;"><span id='total'></span> kWh</td></tr>
    <tr><td style="text-align:right;">Alarm:</td><td style="color:white;"><span id='alarm'></span></td></tr>
  </table>
  <hr>

  <p> 
  <input type="button" class="button" value="Alarm On" onclick="button_clicked('p_on')">
  <input type="button" class="button" value="Alarm Off" onclick="button_clicked('p_off')">
  <input type="button" class="button" value=" Reset " onclick="reset_clicked()"> 
  </p>

  
  <script>
 
    requestData(); // get intial data straight away 

    // request data updates every 1000 milliseconds
    setInterval(requestData, 1000);
    
    function requestData() {

      var xhr = new XMLHttpRequest();
      
      xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {

          if (this.responseText) { // if the returned data is not null, update the values

            var data = JSON.parse(this.responseText);

            document.getElementById("voltage").innerText = data.voltage;
            document.getElementById("current").innerText = data.current;
            document.getElementById("power").innerText = data.power;
            document.getElementById("total").innerText = data.total;            
            document.getElementById("alarm").innerText = data.alarm;            
            document.getElementById("temp").innerText = data.temp;            
            document.getElementById("volt").innerText = data.volt;            
          } 
        } 
      }
      xhr.open('GET', 'get_pow', true);
      xhr.send();
    }

    function button_clicked(key) { 
      var xhr = new XMLHttpRequest();
      xhr.open('GET', key);
      xhr.send();
      requestData();
    }

    function reset_clicked() { 
      if (confirm("Reset Verbrauch?")) {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', 'p_reset');
        xhr.send();
      }      
    }
   
  </script>

</body>

</html>

)=====";
