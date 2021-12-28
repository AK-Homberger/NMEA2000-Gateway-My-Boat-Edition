const char MAIN_page[] PROGMEM = R"=====(
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

<title>Gateway</title>
<hr>
<h1>Boat Control</h1>
<hr>

</head>

<body style="font-family: verdana,sans-serif" BGCOLOR="#819FF7">

<p>
 <br>
 <a href="/nmea">Navigation</a>
 <br>
 <br>
 <a href="/power">Power Control</a>
 <br>
 <br>
 <a href="/heater">Heater Control</a>
 <br>
 <br>
 <a href="/aws">Wind Big</a>   
 <br>
 <br>
 </p>
 <hr>
</body>

</html>

)=====";
