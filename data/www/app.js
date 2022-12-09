$(document).ready(function() {
  $("#networkingLink").click(function() {

    $.get("/connect.html", function(data){
      $("#container").html(data);
      $("#connectButton").click(function() {
        var form = {
          'wifiSSID' : $("#wifiSSID").val(),
          'wifiPassword' : $("#wifiPassword").val(),
          'wifiClient' : $("#wifiAPBox").is(":checked")
        }
        $.post("/connect",form,function(data){
          alert(data.result);
        });
      });
    });
  });
  $("#settingsLink").click(function(){
    $.get("/telegram.html", function(data){
      $("#container").html(data);
      $("#telegramButton").click(function() {
        var form = {
          'telegramToken' : $("#telegramToken").val()
        }
        $.post("/telegram",form,function(data){
          alert(data.result);
        });
      });
    });
  });

});

