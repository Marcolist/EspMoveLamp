// EspMoveLamp mechanical prototype v0.9 / DS3225 / mm
part="left_base";
$fn=64;
W=70; D=100; wall=4;
axis_y=44; axis_z=53;
// DS3225 datasheet: body40x20, flange49.5x10, flange length54.5.
servo_body_l=40.6; servo_body_w=20.6;
servo_plate_x=10; // adjustable bracket via base slots
servo_plate_thick=5;
// Horn outer screw radius24 documented; outline clearances remain fit test.
horn_r=24; horn_total=35.8; horn_width=15.8; horn_thickness=6.3;
profile_w=20.5; profile_h=40.5;
cap_d=72; cap_bolt_r=30.5;
cable_slot_width=6; // provisional until actual extrusion slot measured
// Common concealed outlet, away from negative-u servo arm.
module wireport(h=40){translate([10,5,-1])cube([8,6,h]);}
module nutports(){
 // Actual four LED faces: two at +u, one at +v, one at -v.
 for(v=[0,20])translate([6.5,v-cable_slot_width/2,-1])cube([7,cable_slot_width,25]);
 for(v=[-10,30])translate([-cable_slot_width/2,v-3.5,-1])cube([cable_slot_width,7,25]);
}
module wirepath(points){for(i=[0:len(points)-2])hull(){
 for(j=[i,i+1])translate([points[j][0],points[j][1],-0.1])cylinder(d=cable_slot_width,h=3.1);
}}
module wirechannels(){
 wirepath([[10,0],[14,8]]);
 wirepath([[10,20],[14,8]]);
 wirepath([[0,30],[14,24],[14,8]]);
 wirepath([[0,-10],[14,-10],[14,8]]);
}
module profile(){
 offset(r=3) offset(delta=-3)
 polygon([[0, 0], [100, 0], [100.0, 8.0], [99.5, 8.635], [99.0, 9.281], [98.5, 9.938], [98.0, 10.609], [97.5, 11.296], [97.0, 12.0], [96.5, 12.715], [96.0, 13.435], [95.5, 14.162], [95.0, 14.896], [94.5, 15.639], [94.0, 16.393], [93.5, 17.16], [93.0, 17.94], [92.5, 18.736], [92.0, 19.549], [91.5, 20.38], [91.0, 21.232], [90.5, 22.104], [90.0, 23.0], [89.5, 23.932], [89.0, 24.908], [88.5, 25.925], [88.0, 26.978], [87.5, 28.062], [87.0, 29.173], [86.5, 30.307], [86.0, 31.458], [85.5, 32.622], [85.0, 33.794], [84.5, 34.971], [84.0, 36.148], [83.5, 37.319], [83.0, 38.481], [82.5, 39.629], [82.0, 40.759], [81.5, 41.865], [81.0, 42.944], [80.5, 43.99], [80.0, 45.0], [79.5, 45.99], [79.0, 46.978], [78.5, 47.964], [78.0, 48.946], [77.5, 49.922], [77.0, 50.891], [76.5, 51.853], [76.0, 52.805], [75.5, 53.746], [75.0, 54.675], [74.5, 55.591], [74.0, 56.491], [73.5, 57.376], [73.0, 58.243], [72.5, 59.091], [72.0, 59.918], [71.5, 60.725], [71.0, 61.508], [70.5, 62.267], [70.0, 63.0], [69.5, 63.716], [69.0, 64.423], [68.5, 65.12], [68.0, 65.808], [67.5, 66.486], [67.0, 67.152], [66.5, 67.806], [66.0, 68.449], [65.5, 69.078], [65.0, 69.695], [64.5, 70.297], [64.0, 70.885], [63.5, 71.458], [63.0, 72.015], [62.5, 72.557], [62.0, 73.081], [61.5, 73.588], [61.0, 74.077], [60.5, 74.548], [60.0, 75.0], [59.5, 75.436], [59.0, 75.86], [58.5, 76.272], [58.0, 76.673], [57.5, 77.064], [57.0, 77.444], [56.5, 77.815], [56.0, 78.177], [55.5, 78.531], [55.0, 78.876], [54.5, 79.214], [54.0, 79.545], [53.5, 79.869], [53.0, 80.188], [52.5, 80.5], [52.0, 80.808], [51.5, 81.112], [51.0, 81.411], [50.5, 81.707], [50.0, 82.0], [49.5, 82.291], [49.0, 82.579], [48.5, 82.864], [48.0, 83.147], [47.5, 83.426], [47.0, 83.701], [46.5, 83.973], [46.0, 84.24], [45.5, 84.503], [45.0, 84.76], [44.5, 85.013], [44.0, 85.26], [43.5, 85.501], [43.0, 85.736], [42.5, 85.965], [42.0, 86.187], [41.5, 86.401], [41.0, 86.609], [40.5, 86.808], [40.0, 87.0], [39.5, 87.191], [39.0, 87.388], [38.5, 87.589], [38.0, 87.792], [37.5, 87.996], [37.0, 88.199], [36.5, 88.4], [36.0, 88.596], [35.5, 88.786], [35.0, 88.969], [34.5, 89.142], [34.0, 89.304], [33.5, 89.453], [33.0, 89.588], [32.5, 89.707], [32.0, 89.808], [31.5, 89.889], [31.0, 89.95], [30.5, 89.987], [30.0, 90.0], [29.5, 90.0], [29.0, 90.0], [28.5, 90.0], [28.0, 90.0], [27.5, 90.0], [27.0, 90.0], [26.5, 90.0], [26.0, 90.0], [25.5, 90.0], [25.0, 90.0], [24.5, 90.0], [24.0, 90.0], [23.5, 90.0], [23.0, 90.0], [22.5, 90.0], [22.0, 90.0], [21.5, 89.993], [21.0, 89.974], [20.5, 89.943], [20.0, 89.9], [19.5, 89.845], [19.0, 89.78], [18.5, 89.705], [18.0, 89.619], [17.5, 89.524], [17.0, 89.421], [16.5, 89.309], [16.0, 89.189], [15.5, 89.061], [15.0, 88.927], [14.5, 88.786], [14.0, 88.639], [13.5, 88.486], [13.0, 88.329], [12.5, 88.166], [12.0, 88.0], [11.5, 87.81], [11.0, 87.579], [10.5, 87.307], [10.0, 86.997], [9.5, 86.648], [9.0, 86.262], [8.5, 85.841], [8.0, 85.385], [7.5, 84.896], [7.0, 84.376], [6.5, 83.825], [6.0, 83.244], [5.5, 82.636], [5.0, 82.0], [4.5, 81.276], [4.0, 80.409], [3.5, 79.413], [3.0, 78.303], [2.5, 77.091], [2.0, 75.792], [1.5, 74.42], [1.0, 72.988], [0.5, 71.51], [0.0, 70.0]]);
}

module extrude_x(l){rotate([90,0,90])linear_extrude(l)children();}
module xhole(x,y,z,d,l){translate([x,y,z])rotate([0,90,0])cylinder(d=d,h=l);}
module zhole(x,y,z,d,l){translate([x,y,z])cylinder(d=d,h=l);}
module zslot(x1,x2,y,z,d,h){hull(){zhole(x1,y,z,d,h);zhole(x2,y,z,d,h);}}
module base(right=true){difference(){union(){
 linear_extrude(6)offset(r=2)offset(delta=-2)square([70,100]);
 // Hood attachment lugs, accessible horizontally from outside.
 for(x=[8.3,61.7],y=[12,80])translate([x-4,y-5,6])cube([8,10,12]);
 if(!right){
 // Simple integral axle pedestal, completely exposed without hood.
 translate([7,32,6])cube([8,24,56]);
 for(y=[32,52])hull(){translate([7,y,6])cube([30,4,4]);translate([7,y,6])cube([8,4,35]);}
 }
 }
 for(y=[12,80]){xhole(-1,y,12,2.6,13);xhole(58,y,12,2.6,13);}
 if(right){
 // M4 through slots allow the bracket to move 12mm along servo axis.
 for(y=[28,70])zslot(29,41,y,-1,4.5,8);
 }else{xhole(6,axis_y,axis_z,5.5,11);xhole(12,axis_y,axis_z,11,5);}
 // Anti-slip pad pockets on underside.
 for(x=[13,57],y=[15,80])zhole(x,y,-0.1,10,0.9);
}}
module servo_bracket(){difference(){union(){
 // Part in assembly coordinates, foot underside z6.
 translate([13,19,6])cube([44,55.5,6]);
 translate([servo_plate_x,0,0])extrude_x(servo_plate_thick)
 polygon([[5,12],[65,12],[65,63],[55,70],[5,70]]);
 // Buttress ribs stay below servo body and mounting ears.
 for(y=[20,62])hull(){translate([servo_plate_x,y,12])cube([30,3,3]);translate([servo_plate_x,y,12])cube([5,3,25]);}
 }
 // Clearance above forward hood attachment lug.
 translate([-1,0,5])cube([72,18.5,14]);
 translate([servo_plate_x-1,34-servo_body_l/2,53-servo_body_w/2])cube([7,servo_body_l,servo_body_w]);
 for(y=[34-49.5/2,34+49.5/2],z=[48,58])xhole(servo_plate_x-1,y,z,3.5,8);
 for(y=[28,70])zhole(35,y,5,4.5,9);
}}
module hood(right=true){difference(){
 extrude_x(70)profile();
 // Hollow right through lower edge, no hidden bottom membrane.
 translate([4,0,0])extrude_x(62)offset(delta=-4)profile();
 translate([-1,-1,-1])cube([72,102,7]);
 // U opening permits lifting hood off the installed spindle.
 hull(){xhole(-1,axis_y,axis_z,right?50:10,7);xhole(-1,axis_y,0,right?50:10,7);}
 // Radial cosmetic relief, not a bearing surface.
 xhole(-0.1,axis_y,axis_z,73.2,1.6);
 for(y=[12,80]){xhole(-1,y,12,3.4,6);xhole(65,y,12,3.4,6);}
 // Cable exit aligned with rear lower edge, connector accessible.
 translate([30,94,5])cube([10,8,8]);
}}
module capbolts(){for(a=[45,135,225,315])translate([cap_bolt_r*cos(a),cap_bolt_r*sin(a),-1])cylinder(d=3.4,h=18);}
module rear_servo(){difference(){
 cylinder(d=cap_d,h=13);
 // Pocket OPENS TO SERVO at z0. Original metal horn engages spline there.
 translate([-27.9,-horn_width/2,-0.1])cube([horn_total,horn_width,6.4]);
 // Hub + retaining-screw access. Diameter must be fit-checked on actual horn.
 zhole(0,0,-1,18,16);
 // Fixed outer horn hole at documented radius24; accessible from profile side.
 zhole(-horn_r,0,-1,3.4,16);
 capbolts();wireport();
 for(a=[45,135,225,315])translate([cap_bolt_r*cos(a),cap_bolt_r*sin(a),-0.1])cylinder(d=6.6,h=2.6,$fn=6);
}}
module rear_axle(){difference(){cylinder(d=cap_d,h=13);zhole(0,0,-1,5.5,16);capbolts();wireport();for(a=[45,135,225,315])translate([cap_bolt_r*cos(a),cap_bolt_r*sin(a),-0.1])cylinder(d=6.6,h=2.6,$fn=6);}}
module cap_front(){difference(){union(){
 // 4mm end-stop keeps actual extrusion out of wire-routing plenum.
 cylinder(d=cap_d,h=8);
 translate([0,10,8])linear_extrude(12)difference(){square([26,46],center=true);square([profile_w,profile_h],center=true);}
 }
 translate([0,10,4])linear_extrude(20)square([profile_w,profile_h],center=true);
 // Main bolt pattern must pass through full8mm plate.
 for(a=[45,135,225,315])translate([cap_bolt_r*cos(a),cap_bolt_r*sin(a),-1])cylinder(d=3.4,h=12);
 // Blind underside pocket clears installed M3 screw head; outside stays closed.
 zhole(-horn_r,0,-0.1,9,3.6);
 // Horn fastener is mounted BEFORE closing the two cap halves.
 zhole(0,0,-1,10,11);
 nutports();wirechannels(); // blind channels: no extra outward square outlet
 for(a=[45,135,225,315])translate([cap_bolt_r*cos(a),cap_bolt_r*sin(a),6])cylinder(d=6.3,h=3);
 for(v=[0,20])translate([-15,v,14])rotate([0,90,0])cylinder(d=4.5,h=6);
}}
// Electronics housing: local x=0 faces light bar; export mirrored for LEFT.
LW=160; LD=190; LH=100;
psu=[110,80,36]; psu_pos=[20,75,10];
board_max=[64,34]; // QIQIAZI 38pin CP2102 USB-C: actual PCB size not supplied board_pos=[80,20,10];
module large_profile(){
 // Smooth rear slope; retains clearance above PSU rear edge at y155.
 pts=concat([[0,0],[190,0],[190,8]],
 [for(i=[0:100])let(t=i/100)
 [pow(1-t,3)*190+3*pow(1-t,2)*t*168+3*(1-t)*t*t*135+pow(t,3)*38,
 pow(1-t,3)*8+3*pow(1-t,2)*t*95+3*(1-t)*t*t*100+pow(t,3)*100]],
 [for(i=[1:45])let(t=i/45)
 [pow(1-t,3)*38+3*pow(1-t,2)*t*10,
 pow(1-t,3)*100+3*pow(1-t,2)*t*100+3*(1-t)*t*t*95+pow(t,3)*74]],[[0,0]]);
 offset(r=2)offset(delta=-2)polygon(pts);
}
module lip_fill(){
 // Fills U opening below the round50mm cable/spindle opening.
 difference(){translate([0.3,axis_y-24.6,6])cube([3.4,49.2,47]);
 xhole(-1,axis_y,axis_z,50,7);}
}
module electronics_base(){difference(){union(){
 linear_extrude(6)offset(r=3)offset(delta=-3)square([LW,LD]);
 lip_fill();
 // Hood supports. Rear posts sit before low tail of shell.
 for(x=[8.3,LW-8.3],y=[12,150])translate([x-4,y-5,6])cube([8,10,12]);
 // PSU rests4mm above base; no guessed threaded holes in metal PSU.
 for(y=[75,145])translate([20,y,6])cube([110,10,4]);
 for(x=[17,130],y=[75,149])translate([x,y,6])cube([3,6,9]);
 // Strap fastening pedestals; thermal/mains suitability not certified.
 for(x=[30,120],y=[68,162])translate([x-5,y-5,6])cube([10,10,4]);
 // Separate low-voltage partition, open routing at front.
 translate([70,64,6])cube([83,3,44]);
 // Universal ESP carrier screw seats. Board-size assumptions exposed above.
 for(x=[76,148],y=[24,50])translate([x-4,y-4,6])cube([8,8,4]);
 // Terminal-shroud fastening feet, outside PSU envelope.
 for(y=[68,162])translate([6,y-4,6])cube([10,8,4]);
 }
 // Existing servo mount, same axle height and offset as compact right side.
 for(y=[28,70])zslot(29,41,y,-1,4.5,8);
 for(y=[12,150]){xhole(-1,y,12,2.6,13);xhole(LW-12,y,12,2.6,13);}
 for(x=[30,120],y=[68,162])zhole(x,y,5,2.6,7);
 for(x=[76,148],y=[24,50])zhole(x,y,5,2.6,7);
 for(y=[68,162])zhole(11,y,5,2.6,7);
 for(x=[14,146],y=[15,171])zhole(x,y,-0.1,12,0.9);
}}
module electronics_hood(){difference(){
 extrude_x(LW)large_profile();
 translate([4,0,0])extrude_x(LW-8)offset(delta=-4)large_profile();
 translate([-1,-1,-1])cube([LW+2,LD+2,7]);
 hull(){xhole(-1,axis_y,axis_z,50,7);xhole(-1,axis_y,0,50,7);}
 xhole(-0.1,axis_y,axis_z,73.2,1.6);
 for(y=[12,150]){xhole(-1,y,12,3.4,6);xhole(LW-5,y,12,3.4,6);}
 // Low-voltage-zone wall vents. Terminal area remains unperforated.
 for(y=[24:9:60])translate([LW-5,y,24])cube([7,3,22]);
 // Rear exhaust above protected metal PSU housing, not above front terminals.
 for(x=[62:10:142])translate([x,100,53])cube([3,53,60]);
 // Cable-entry pilot on outer side in terminal-shroud area.
 // Real strain-relief fitting determines finished diameter; not final mains entry.
 xhole(-1,115,18,6,7);
}}
module psu_strap(){difference(){union(){
 // One strap spans y63..167; two are installed at x30 and120.
 translate([-4,63,46.8])cube([8,104,3]);
 for(y=[64,158])translate([-4,y,10])cube([8,8,39.8]);
 for(y=[62,158])translate([-7,y,10])cube([14,10,3]);
 }
 for(y=[68,162]){zhole(0,y,9,3.4,45);zhole(0,y,46,6.2,5);}
}}
module esp_carrier(){difference(){union(){
 // No fixed DevKit drilling pattern: supports edges of a provisional64x34 PCB.
 translate([72,16,10])cube([80,42,3]);
 for(y=[18,53])translate([80,y,13])cube([64,3,3]);
 }
 // Open underside area avoids covering all board solder joints.
 translate([84,25,9])cube([56,24,5]);
 for(x=[76,148],y=[24,50])zhole(x,y,9,3.4,6);
 // Slots for two small printable retaining fingers at PCB ends.
 for(x=[82,142])zslot(x-3,x+3,37,9,3.4,7);
}}
module esp_finger(){difference(){union(){cube([10,8,3]);translate([0,0,3])cube([4,8,4]);translate([0,0,7])cube([8,8,2]);}zhole(7,4,-1,3.4,5);}}
module terminal_shroud(){difference(){union(){
 // Secondary terminal barrier; open rear fits over metal PSU edge.
 translate([5,71,6])cube([43,88,46]);
 for(y=[68,162])translate([5,y-4,10])cube([12,8,4]);
 }
 translate([8,74,5])cube([42,82,44]);
 for(y=[68,162])zhole(11,y,9,3.4,7);
 xhole(-1,115,18,6,12);
 // Pilot only: actual rated cable fitting determines final opening.
}}
module left_flip(){mirror([1,0,0])translate([-LW,0,0])children();}
if(part=="left_base")left_flip()electronics_base();
if(part=="left_hood")left_flip()electronics_hood();
if(part=="servo_mount")left_flip()translate([0,0,-6])servo_bracket();
if(part=="right_base"){base(false);small_axle_fill();}
if(part=="right_hood")hood(false);
if(part=="horn_cap")rear_servo();
if(part=="axle_cap")rear_axle();
if(part=="profile_cap")cap_front();
if(part=="psu_strap")translate([0,-62,-10])psu_strap();
if(part=="esp_carrier")translate([-72,-16,-10])esp_carrier();
if(part=="esp_finger")esp_finger();
if(part=="terminal_shroud")translate([-5,-64,-6])terminal_shroud();
// Non-printable reference parts / inspection, only for collision and layout.
module psu_reference(){translate(psu_pos)cube(psu);}
module esp_reference(){translate([80,20,16])cube([64,34,12]);}
module servo_reference(){translate([15,14,43])cube([27.7,40,20]);}
module installed_parts(){
 electronics_base();servo_bracket();esp_carrier();terminal_shroud();
 for(x=[30,120])translate([x,0,0])psu_strap();
}
if(part=="inspection")left_flip(){installed_parts();psu_reference();esp_reference();servo_reference();}
if(part=="collision_hood_parts")intersection(){electronics_hood();installed_parts();}
if(part=="collision_hood_psu")intersection(){electronics_hood();psu_reference();}
if(part=="collision_hood_esp")intersection(){electronics_hood();esp_reference();}
if(part=="collision_shroud_psu")intersection(){terminal_shroud();psu_reference();}
module small_axle_fill(){difference(){translate([0.3,axis_y-4.6,6])cube([3.4,9.2,47]);xhole(-1,axis_y,axis_z,10,7);}}
