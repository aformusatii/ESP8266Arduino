var co2,aiq2_5,aiq10_0,rad;

var loadCO2 = function() {
  $.ajax({
   type: "GET",
   url: "/sensor/1",
   success: function(respose) {
     co2.add(respose.value);
   }
  });
}

var loadPM = function() {
  $.ajax({
   type: "GET",
   url: "/sensor/2",
   success: function(respose) {
     aiq2_5.add(respose.pm_2_5);
     aiq10_0.add(respose.pm_10_0);
   }
  });
}

var loadRad = function() {
  $.ajax({
   type: "GET",
   url: "/sensor/3",
   success: function(respose) {
     rad.add(respose.value);
   }
  });
}

$(function() {
  
  setInterval(loadCO2, 60000);
  setInterval(loadPM, 61000);
  setInterval(loadRad, 62000);
  
  loadCO2();
  loadPM();
  loadRad();
  
  co2 = createGraph(document.getElementById("co2"),         "CO2 (800 norm)",                100, 128, 200, 2000, false, "cyan");
  aiq2_5 = createGraph(document.getElementById("aiq2_5"),   "PM2.5 ug/m3 (12 norm)",  100, 125,   0, 100,  true,  "orange");
  aiq10_0 = createGraph(document.getElementById("aiq10_0"), "PM10.0 ug/m3 (40 norm)", 100, 125,   0, 100,  true,  "orange");
  rad = createGraph(document.getElementById("rad"),         "RAD CPM (24 norm)",          100, 125,   0, 100,  true,  "gold");
});