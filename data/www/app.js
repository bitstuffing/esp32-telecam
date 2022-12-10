$(document).ready(function() {
  $(".curtain").fadeOut("fast");
  $(".container-fluid").fadeIn("fast");
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
    $("#container").html("");
    $.get("/telegram.html", function(data){
      $("#container").append(data);
      $("#telegramButton").click(function() {
        var form = {
          'telegramToken' : $("#telegramToken").val()
        }
        $.post("/telegram",form,function(data){
          alert(data.result);
        });
      });
    });
    $.get("/auth.html", function(data){
      $("#container").append(data);
      $.get("/getAuthUsers",function(data2){
        $.each(data2,function(k,v){
          var option = $("<option value='"+v+"'>"+v+"</option>");
          $("#authUsers").append(option);
        });
        $("#addAuthUsersButton").click(function(){
          var value = $("#newAuthUser").val();
          var form = {'newUser' : value };
          $.post("/addAuthUser",form,function(resp){
            if(resp.result.toLowerCase()=='ok'){
              var v = $("#newAuthUser").val();
              var option = $("<option value='"+value+"'>"+value+"</option>");
              $("#authUsers").append(option);
              $("#newAuthUser").val("");
            }else{
              alert("Not possible, try again with other username.");  
            }
          });
        });
        $("#removeAuthUsersButton").click(function(){
          if($("#authUsers option").length>1){
            var value = $("#authUsers").find(":selected").val();
            if($("#authUsers").find(":selected").length>0){
              var form = {'delUser' : value };
              $.post("/removeAuthUser",form,function(resp){
                if(resp.result.toLowerCase()=='ok'){
                  $("#authUsers option[value='"+$("#authUsers").val()+"']").remove();
                  $("#authUsers").val("");
                }
              });
            }else{
              alert("You've not selected anybody, select one first.");
            }
          }else{
            alert("You're not able to remove the last user, add other one first.");
          }
        });
      });
    });
  });
});

