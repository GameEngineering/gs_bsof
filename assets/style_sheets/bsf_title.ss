// bsf_title.ss

//=== Constants ===//

// Fonts
$arwing_h0:     "font.arwing_h0";
$arwing_h1:     "font.arwing_h1";
$arwing_h2:     "font.arwing_h2";
$upheaval_h0:   "font.upheaval_h0";
$upheaval_h1:   "font.upheaval_h1";
$upheaval_h2:   "font.upheaval_h2";
$upheaval_p:    "font.upheaval_p";

// Colors
$invisible:     rgba(0 0 0 0);
$blood:         rgb(138 3 3);
$dark_blood:    rgb(60 1 1);
$red:           rgb(255 0 0);
$white:         rgb(255 255 255);
$grey:          rgb(150 150 150);
$black:         rgb(0 0 0);

//=== Elements ==//

* {
    color_background: $invisible;
    // color_border: $invisible;
    justify_content: center;
    color_shadow: rgba(0 0 0 100);
    color_content_shadow: rgba(0 0 0 100);
    shadow: 3;

    // Debugging
    border: 0;
    color_border: $red; 
}

label {
    font: $upheaval_p;
}

text {
    font: $upheaval_p;
}

input {
    color_background: rgba(0 0 0 50);
    justify_content: center;    // Want this to work...
    height: 70;
    border: 2;
    color_border: rgba(0 0 0 100);
    font: $upheaval_h2;
    padding_right: 5; 
    margin_left: -8;
    transition: {
        color_content: 200 0;
        color_shadow: 200 0;
    }
}

input: hover {
    color_content: $dark_blood;
    color_shadow: rgb(100 1 1);
}

input: focus {
    color_content: $red;
    color_shadow: $black;
}

button {
    font: $upheaval_h1;
    width: 150;
    height: 35;
    transition: {
        color_content: 100 0;
    }
}

button : hover {
    color_content: $dark_blood;
}

button : focus {
    color_content: $red;
}

label {
    width: 150; 
} 

container {
    color_background: rgb(100 100 100);
}

container: hover {
    color_background: rgb(150 150 150);
}

container: focus {
    color_background: rgb(150 150 150);
}

//=== Classes ===// 

.seed {
    justify_content: end;    // Want this to work...
    height: 70;
    font: $upheaval_h2;
    padding_right: 5;
    margin_left: -10;
    transition: {
        color_content: 200 0;
        color_shadow: 200 0;
    }
}

.top_panel_item {
    margin_top: 40;
} 

.title_vin {
    color_content: rgba(255 255 255 150);
}

.title_bg {
    color_content: rgb(180 180 180);
}

.bsfpanel {
    // border: 1;
    // color_border: rgba(0 0 0 20);
}

.start_lbl {
    color_content: rgb(50 50 50);
    margin_top: 20;
    font: $upheaval_h1;
}

.margin_title {
    padding_left: 10;
    margin_left: 100;
    margin_top: 50; 
    margin_bottom: 0;
}

.hover_red {
    shadow: 4;
    transition: {
        color_content: 100 0;
    }
}

.hover_red: hover {
    color_content: $dark_blood;
}

.hover_red: focus {
    color_content: $red;
}

//=== IDs ===// 

#title_bind {
    color_content: $dark_blood;
    width: 400;
    height: 30;
    font: $upheaval_h1;
    align_content: end;
} 

#title_sf {
    color_background: rgba(0 0 0 0);
    color_content: $blood;
    width: 400;
    height: 80;
    font: $arwing_h0;
} 

#title_sf: focus {
    color_content: $blood;
}



