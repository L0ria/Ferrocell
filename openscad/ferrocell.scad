$fn=128;

fc_dia=91.4;
fc_height=17;

led_width=15;
rim_addon=5;
led_depth=2;
led_rim=5;
led_wall=1;

bat_x=100;
bat_y=30;
bat_z=28;
bat_usb_loc_x=72;
bat_usb_loc_y=bat_y;
bat_usb_loc_z=21+5;
bat_button_x=3;
bat_button_y=1.2;
bat_button_z=1.2;
bat_button_loc_x=10;
bat_button_loc_y=bat_y;
bat_button_loc_z=15+6;
bat_led_x=16;
bat_led_y=10;
bat_led_z=3;
bat_led_loc_x=20;
bat_led_loc_y=bat_y;
bat_led_loc_z=20+5;
bat_bscrew_loc=[[3,2],[98,2],[3,27],[98,27]];

switch_x=32;
switch_y=20;
switch_z=12;

esp_x=24;
esp_y=30;
esp_z=19;

usb_x=9;
usb_y=5;
usb_z=3.5;

esp_usb_loc_x=5.5;
esp_usb_loc_y=esp_y;
esp_usb_loc_z=15.5;
esp_bscrew_loc=[[21,2],[21,27]];

cable_opening_x=10;

port_gap=1;
gap=0.8;
wall=4;
case_wall=4;

bscrew_dia=2;
bscrew_height=4;
screw_space=10;

cscrew_dia=3;

rim=wall+rim_addon;
height=2*wall+led_wall+led_width+fc_height;
dia=fc_dia+2*rim;
cable_opening_y=dia/2;

// case_x=bat_x+2*case_wall;
case_x=dia;
case_y=max(bat_y,esp_y,switch_y)+2*case_wall;
case_z=bat_z+esp_z+case_wall+2;
case_screw_z=10;
case_screw_loc=[[dia/2-3.5,0],
                [-dia/2+3.5,0],
                [dia/2-3.5,dia/2+case_y-2*case_wall],
                [-dia/2+3.5,dia/2+case_y-2*case_wall],
                [dia/2-3.5,dia/2-2*case_wall],
                [-dia/2+3.5,dia/2-2*case_wall],
                [0,-dia/2+3.5]];


//main_case();
//bottom();
top();

module top()
{
    difference()
    {
        body_frame(height=case_wall);
        case_screws(dia=cscrew_dia+0.6,head=true);
        cylinder(d=fc_dia-1,h=case_wall);
    }
}


module bottom()
{
    difference()
    {
        body_frame(height=case_wall);
        case_screws(dia=cscrew_dia+0.6,head=true);
    }
    translate([0,0,case_wall])
    {
        difference()
        {
            cylinder(d=fc_dia-gap,h=1.2);
            cylinder(d=fc_dia-2*case_wall,h=1.2);
        }
    }
}

// body frame
module body_frame(height)
{
    cylinder(d=dia,h=height);
    translate([-case_x/2,0-case_wall,0])
    {
        cube([case_x,case_y+dia/2,height]);
    }
}

module case_screws(dia=10,head=false)
{
    for(loc=case_screw_loc)
    {
        translate([loc[0],loc[1],0])
        {
            #cylinder(d=dia,h=case_screw_z);
            if(head)
            {
                #cylinder(d=1.6*dia,h=1);
                translate([0,0,1])
                {
                    #cylinder(d1=1.6*dia,d2=dia,h=1);
                }
            }
        }
    }
}

// esp
module esp()
{
    translate([0,0,0])
    {
        cube([esp_x,esp_y,esp_z]);
        translate([esp_usb_loc_x-port_gap,esp_usb_loc_y,esp_usb_loc_z-port_gap])
        {
            cube([usb_x+2*port_gap,usb_y,usb_z+2*port_gap]);
            translate([-case_wall,usb_y-2,-case_wall])
            {
                cube([usb_x+2*port_gap+2*case_wall,usb_y,usb_z+2*port_gap+2*case_wall]);
            }
        }
        for(loc=esp_bscrew_loc)
        {
            translate([loc[0],loc[1],-bscrew_height])
            {
                cylinder(d=bscrew_dia,h=bscrew_height);
            }
        }
    }
}

module battery()
{
    cube([bat_x,bat_y,bat_z]);
    translate([bat_usb_loc_x-port_gap,bat_usb_loc_y,bat_usb_loc_z-port_gap])
    {
        cube([usb_x+2*port_gap,usb_y,usb_z+2*port_gap]);
        translate([-case_wall,usb_y-2,-case_wall])
        {
            cube([usb_x+2*port_gap+2*case_wall,usb_y,usb_z+2*port_gap+2*case_wall]);
        }
    }
    translate([bat_button_loc_x-port_gap,bat_button_loc_y,bat_button_loc_z-port_gap])
    {
        cube([bat_button_x+2*port_gap,bat_button_y,bat_button_z+2*port_gap]);
        translate([-case_wall,bat_button_y,-case_wall])
        {
            cube([bat_button_x+2*port_gap+2*case_wall,10,bat_button_z+2*port_gap+2*case_wall]);
        }
    }
    translate([bat_led_loc_x,bat_led_loc_y,bat_led_loc_z])
    {
        cube([bat_led_x,bat_led_y,bat_led_z]);
    }
    for(loc=bat_bscrew_loc)
    {
        translate([loc[0],loc[1],bat_z])
        {
            cylinder(d=bscrew_dia,h=bscrew_height);
        }
    }
}

module main_case()
{
    difference()
    {
        body_frame(height=case_z);
        // battery cutout
        translate([case_wall-case_x/2-gap,dia/2-case_wall-gap,0])
        {
            cube([case_x-2*case_wall+2*gap,case_y-2*case_wall+2*gap,bat_z]);
        }
        // esp cutout
        translate([case_wall-case_x/2-gap,dia/2-case_wall-gap,bat_z+case_wall])
        {
            cube([case_x-2*case_wall+2*gap,case_y-2*case_wall+2*gap,esp_z+2]);
        }
        // ferrocell_cutout
        translate([0,0,case_z-fc_height])
        {
            cylinder(d=fc_dia,h=fc_height);
        }
        // middle hole
        translate([0,0,case_z-fc_height-wall])
        {
            cylinder(d=fc_dia-wall,h=wall);
        }
        // lower led cutout
        cylinder(d=fc_dia+2*led_depth,h=case_z-fc_height-wall);
        // opening for cables
        translate([-cable_opening_x/2+case_x/4,0,0])
        {
            cube([cable_opening_x,cable_opening_y,case_z-fc_height-wall]);
            translate([0,dia/2-3*case_wall,0])
            {
                cube([cable_opening_x,2*case_wall,case_z]);
            }
        }
        // cutout between boards
        translate([-case_x/2+esp_x+screw_space+case_wall,dia/2-case_wall-gap,bat_z])
        {
            cube([case_x-2*case_wall-2*screw_space-esp_x,case_y-2*case_wall+2*gap,wall]);
        }
        // cutout for switch
        translate([-switch_x+dia/2,dia/2,case_z-switch_z-case_wall])
        {
            cube([switch_x,switch_y,switch_z]);
            translate([switch_x-case_wall/2,-3,-3])
            {
                cube([case_wall/2,switch_y+2*3,switch_z+2*3]);
            }
        }
        // esp cutouts
        translate([-case_x/2+case_wall+screw_space,dia/2-case_wall,bat_z+case_wall])
        {
            esp();
        }
        
        // battery cutouts
        translate([case_wall-case_x/2,dia/2-case_wall,0])
        {
            battery();
        }
        case_screws(dia=cscrew_dia,head=false);
        translate([0,0,case_z-case_screw_z])
        {
            case_screws(dia=cscrew_dia,head=false);
        }
    }
}