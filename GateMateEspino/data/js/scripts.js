
InitWebSocket();
function InitWebSocket(){
websock=new WebSocket('ws://'+window.location.hostname+':88/');

}

function button(){
websock.send('wtf='+123);
}

function Slider(){
sliderVal=slider.noUiSlider.get()
websock.send('sliderVal='+sliderVal);
}

function Slider2(){
sliderVal2=range.noUiSlider.get()
websock.send('sliderVal2='+sliderVal2);
}


