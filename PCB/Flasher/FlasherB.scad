// Generated case design for Flasher/Flasher.kicad_pcb
// By https://github.com/revk/PCBCase
// Generated 2025-10-12 12:30:40
// title:	Flasher
// rev:	1
// company:	Adrian Kennard, Andrews & Arnold Ltd
//

// Globals
margin=0.250000;
lip=3.000000;
lipa=0;
lipt=2;
casewall=3.000000;
casebottom=3.000000;
casetop=10.000000;
bottomthickness=0.000000;
topthickness=0.000000;
fit=0.000000;
snap=0.150000;
edge=2.000000;
pcbthickness=1.600000;
function pcbthickness()=1.600000;
nohull=false;
hullcap=1.000000;
hulledge=1.000000;
useredge=false;
datex=0.000000;
datey=0.000000;
datet=0.500000;
dateh=3.000000;
datea=0;
date="2025-10-12";
datef="OCRB";
spacing=86.000000;
pcbwidth=70.000000;
function pcbwidth()=70.000000;
pcblength=30.000000;
function pcblength()=30.000000;
originx=135.000000;
originy=65.000000;

module outline(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[-7.350000,15.000000],[-35.000000,15.000000],[-35.000000,-15.000000],[35.000000,-15.000000],[35.000000,15.000000],[6.250000,15.000000],[6.217406,14.710723],[6.121260,14.435951],[5.966381,14.189463],[5.760537,13.983619],[5.514049,13.828740],[5.239277,13.732594],[4.950000,13.700000],[-6.050000,13.700000],[-6.339277,13.732594],[-6.614049,13.828740],[-6.860537,13.983619],[-7.066381,14.189463],[-7.221260,14.435951],[-7.317406,14.710723]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19]]);}

module pcb(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[-7.350000,15.000000],[-35.000000,15.000000],[-35.000000,-15.000000],[35.000000,-15.000000],[35.000000,15.000000],[6.250000,15.000000],[6.217406,14.710723],[6.121260,14.435951],[5.966381,14.189463],[5.760537,13.983619],[5.514049,13.828740],[5.239277,13.732594],[4.950000,13.700000],[-6.050000,13.700000],[-6.339277,13.732594],[-6.614049,13.828740],[-6.860537,13.983619],[-7.066381,14.189463],[-7.221260,14.435951],[-7.317406,14.710723]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19]]);}
module J4(){translate([29.080000,0.000000,1.600000])rotate([0,0,90.000000])children();}
module part_J4(part=true,hole=false,block=false)
{
translate([29.080000,0.000000,1.600000])rotate([0,0,90.000000])translate([0.000000,-1.900000,0.000000])rotate([90.000000,0.000000,0.000000])m0(part,hole,block,casetop); // RevK:USB-C-Socket-H CSP-USC16-TR (back)
};
module R14(){translate([-2.650000,-4.100000,1.600000])rotate([0,0,90.000000])children();}
module part_R14(part=true,hole=false,block=false)
{
translate([-2.650000,-4.100000,1.600000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module C1(){translate([22.000000,-8.500000,1.600000])rotate([0,0,-90.000000])children();}
module part_C1(part=true,hole=false,block=false)
{
translate([22.000000,-8.500000,1.600000])rotate([0,0,-90.000000])m2(part,hole,block,casetop); // RevK:C_0805 C_0805_2012Metric (back)
};
module D10(){translate([-14.000000,8.975000,1.600000])children();}
module part_D10(part=true,hole=false,block=false)
{
translate([-14.000000,8.975000,1.600000])m3(part,hole,block,casetop); // D10 (back)
};
module D14(){translate([-19.000000,-3.000000,1.600000])rotate([0,0,180.000000])children();}
module part_D14(part=true,hole=false,block=false)
{
translate([-19.000000,-3.000000,1.600000])rotate([0,0,180.000000])m4(part,hole,block,casetop); // D14 (back)
};
module U5(){translate([-19.500000,-10.500000,1.600000])rotate([0,0,180.000000])children();}
module part_U5(part=true,hole=false,block=false)
{
translate([-19.500000,-10.500000,1.600000])rotate([0,0,180.000000])m5(part,hole,block,casetop); // U5 (back)
};
module R11(){translate([2.850000,-4.100000,1.600000])rotate([0,0,90.000000])children();}
module part_R11(part=true,hole=false,block=false)
{
translate([2.850000,-4.100000,1.600000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module R10(){translate([32.100000,-8.000000,1.600000])rotate([0,0,-90.000000])children();}
module part_R10(part=true,hole=false,block=false)
{
translate([32.100000,-8.000000,1.600000])rotate([0,0,-90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module SW1(){translate([-4.100000,-8.000000,1.600000])rotate([0,0,180.000000])children();}
module part_SW1(part=true,hole=false,block=false)
{
translate([-4.100000,-8.000000,1.600000])rotate([0,0,180.000000])m6(part,hole,block,casetop); // SW1 (back)
};
module V3(){translate([0.000000,15.000000,1.600000])children();}
module part_V3(part=true,hole=false,block=false)
{
};
module D1(){translate([-14.000000,-9.025000,1.600000])children();}
module part_D1(part=true,hole=false,block=false)
{
translate([-14.000000,-9.025000,1.600000])m3(part,hole,block,casetop); // D10 (back)
};
module C35(){translate([7.000000,-8.500000,1.600000])rotate([0,0,-90.000000])children();}
module part_C35(part=true,hole=false,block=false)
{
translate([7.000000,-8.500000,1.600000])rotate([0,0,-90.000000])m7(part,hole,block,casetop); // C35 (back)
};
module R4(){translate([25.600000,-6.600000,1.600000])rotate([0,0,180.000000])children();}
module part_R4(part=true,hole=false,block=false)
{
translate([25.600000,-6.600000,1.600000])rotate([0,0,180.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module D4(){translate([-14.000000,-3.025000,1.600000])children();}
module part_D4(part=true,hole=false,block=false)
{
translate([-14.000000,-3.025000,1.600000])m3(part,hole,block,casetop); // D10 (back)
};
module R3(){translate([26.350000,-5.800000,1.600000])rotate([0,0,-90.000000])children();}
module part_R3(part=true,hole=false,block=false)
{
translate([26.350000,-5.800000,1.600000])rotate([0,0,-90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module C13(){translate([30.900000,-8.000000,1.600000])rotate([0,0,90.000000])children();}
module part_C13(part=true,hole=false,block=false)
{
translate([30.900000,-8.000000,1.600000])rotate([0,0,90.000000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module R6(){translate([3.950000,-4.100000,1.600000])rotate([0,0,90.000000])children();}
module part_R6(part=true,hole=false,block=false)
{
translate([3.950000,-4.100000,1.600000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module R9(){translate([31.500000,-8.000000,1.600000])rotate([0,0,-90.000000])children();}
module part_R9(part=true,hole=false,block=false)
{
translate([31.500000,-8.000000,1.600000])rotate([0,0,-90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module C10(){translate([7.500000,4.900000,1.600000])rotate([0,0,90.000000])children();}
module part_C10(part=true,hole=false,block=false)
{
translate([7.500000,4.900000,1.600000])rotate([0,0,90.000000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module J2(){translate([-28.000000,-7.500000,1.600000])rotate([0,0,-90.000000])children();}
module part_J2(part=true,hole=false,block=false)
{
translate([-28.000000,-7.500000,1.600000])rotate([0,0,-90.000000])m9(part,hole,block,casetop); // J2 (back)
};
module R7(){translate([30.780000,-5.200000,1.600000])children();}
module part_R7(part=true,hole=false,block=false)
{
translate([30.780000,-5.200000,1.600000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module D6(){translate([-14.000000,0.975000,1.600000])children();}
module part_D6(part=true,hole=false,block=false)
{
translate([-14.000000,0.975000,1.600000])m3(part,hole,block,casetop); // D10 (back)
};
module D7(){translate([-14.000000,2.975000,1.600000])children();}
module part_D7(part=true,hole=false,block=false)
{
translate([-14.000000,2.975000,1.600000])m3(part,hole,block,casetop); // D10 (back)
};
module V2(){translate([0.000000,-15.000000,1.600000])rotate([0,0,180.000000])children();}
module part_V2(part=true,hole=false,block=false)
{
};
module C8(){translate([7.450000,7.775000,1.600000])rotate([0,0,-90.000000])children();}
module part_C8(part=true,hole=false,block=false)
{
translate([7.450000,7.775000,1.600000])rotate([0,0,-90.000000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module R15(){translate([-3.750000,-4.100000,1.600000])rotate([0,0,90.000000])children();}
module part_R15(part=true,hole=false,block=false)
{
translate([-3.750000,-4.100000,1.600000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module C16(){translate([-12.275000,-6.550000,1.600000])children();}
module part_C16(part=true,hole=false,block=false)
{
translate([-12.275000,-6.550000,1.600000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module D12(){translate([-18.950000,7.500000,1.600000])rotate([0,0,180.000000])children();}
module part_D12(part=true,hole=false,block=false)
{
translate([-18.950000,7.500000,1.600000])rotate([0,0,180.000000])m4(part,hole,block,casetop); // D14 (back)
};
module PCB1(){translate([0.000000,0.000000,1.600000])children();}
module part_PCB1(part=true,hole=false,block=false)
{
};
module C14(){translate([-12.275000,5.450000,1.600000])children();}
module part_C14(part=true,hole=false,block=false)
{
translate([-12.275000,5.450000,1.600000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module D11(){translate([-8.300000,13.500000,1.600000])children();}
module part_D11(part=true,hole=false,block=false)
{
translate([-8.300000,13.500000,1.600000])m3(part,hole,block,casetop); // D10 (back)
};
module D2(){translate([-14.000000,-7.025000,1.600000])children();}
module part_D2(part=true,hole=false,block=false)
{
translate([-14.000000,-7.025000,1.600000])m3(part,hole,block,casetop); // D10 (back)
};
module D5(){translate([-14.000000,-1.025000,1.600000])children();}
module part_D5(part=true,hole=false,block=false)
{
translate([-14.000000,-1.025000,1.600000])m3(part,hole,block,casetop); // D10 (back)
};
module C3(){translate([24.800000,-7.650000,1.600000])rotate([0,0,90.000000])children();}
module part_C3(part=true,hole=false,block=false)
{
translate([24.800000,-7.650000,1.600000])rotate([0,0,90.000000])m10(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module J5(){translate([-1.500000,5.350000,1.600000])rotate([0,0,180.000000])children();}
module part_J5(part=true,hole=false,block=false)
{
translate([-1.500000,5.350000,1.600000])rotate([0,0,180.000000])m11(part,hole,block,casetop); // J5 (back)
};
module D9(){translate([-14.000000,6.975000,1.600000])children();}
module part_D9(part=true,hole=false,block=false)
{
translate([-14.000000,6.975000,1.600000])m3(part,hole,block,casetop); // D10 (back)
};
module R12(){translate([0.650000,-4.100000,1.600000])rotate([0,0,90.000000])children();}
module part_R12(part=true,hole=false,block=false)
{
translate([0.650000,-4.100000,1.600000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module D8(){translate([-14.000000,4.975000,1.600000])children();}
module part_D8(part=true,hole=false,block=false)
{
translate([-14.000000,4.975000,1.600000])m3(part,hole,block,casetop); // D10 (back)
};
module C11(){translate([1.750000,-4.100000,1.600000])rotate([0,0,-90.000000])children();}
module part_C11(part=true,hole=false,block=false)
{
translate([1.750000,-4.100000,1.600000])rotate([0,0,-90.000000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module C17(){translate([3.000000,-8.500000,1.600000])rotate([0,0,-90.000000])children();}
module part_C17(part=true,hole=false,block=false)
{
translate([3.000000,-8.500000,1.600000])rotate([0,0,-90.000000])m7(part,hole,block,casetop); // C35 (back)
};
module D3(){translate([-14.000000,-5.025000,1.600000])children();}
module part_D3(part=true,hole=false,block=false)
{
translate([-14.000000,-5.025000,1.600000])m3(part,hole,block,casetop); // D10 (back)
};
module C5(){translate([27.450000,-5.800000,1.600000])rotate([0,0,90.000000])children();}
module part_C5(part=true,hole=false,block=false)
{
translate([27.450000,-5.800000,1.600000])rotate([0,0,90.000000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module R2(){translate([24.900000,8.775000,1.600000])rotate([0,0,-90.000000])children();}
module part_R2(part=true,hole=false,block=false)
{
translate([24.900000,8.775000,1.600000])rotate([0,0,-90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module C6(){translate([25.800000,-5.800000,1.600000])rotate([0,0,90.000000])children();}
module part_C6(part=true,hole=false,block=false)
{
translate([25.800000,-5.800000,1.600000])rotate([0,0,90.000000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module U2(){translate([26.900000,-7.500000,1.600000])rotate([0,0,90.000000])children();}
module part_U2(part=true,hole=false,block=false)
{
translate([26.900000,-7.500000,1.600000])rotate([0,0,90.000000])m12(part,hole,block,casetop); // U2 (back)
};
module R13(){translate([-1.550000,-4.100000,1.600000])rotate([0,0,90.000000])children();}
module part_R13(part=true,hole=false,block=false)
{
translate([-1.550000,-4.100000,1.600000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module C7(){translate([22.000000,-12.500000,1.600000])rotate([0,0,90.000000])children();}
module part_C7(part=true,hole=false,block=false)
{
translate([22.000000,-12.500000,1.600000])rotate([0,0,90.000000])m2(part,hole,block,casetop); // RevK:C_0805 C_0805_2012Metric (back)
};
module C15(){translate([-12.275000,-0.550000,1.600000])children();}
module part_C15(part=true,hole=false,block=false)
{
translate([-12.275000,-0.550000,1.600000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module U1(){translate([16.000000,2.000000,1.600000])children();}
module part_U1(part=true,hole=false,block=false)
{
translate([16.000000,2.000000,1.600000])m13(part,hole,block,casetop); // U1 (back)
};
module D13(){translate([-18.950000,5.500000,1.600000])rotate([0,0,180.000000])children();}
module part_D13(part=true,hole=false,block=false)
{
translate([-18.950000,5.500000,1.600000])rotate([0,0,180.000000])m4(part,hole,block,casetop); // D14 (back)
};
module R5(){translate([5.050000,-4.100000,1.600000])rotate([0,0,90.000000])children();}
module part_R5(part=true,hole=false,block=false)
{
translate([5.050000,-4.100000,1.600000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module L1(){translate([25.500000,-11.600000,1.600000])rotate([0,0,-90.000000])children();}
module part_L1(part=true,hole=false,block=false)
{
translate([25.500000,-11.600000,1.600000])rotate([0,0,-90.000000])m14(part,hole,block,casetop); // L1 (back)
};
module U3(){translate([32.000000,-11.100000,1.600000])rotate([0,0,180.000000])children();}
module part_U3(part=true,hole=false,block=false)
{
translate([32.000000,-11.100000,1.600000])rotate([0,0,180.000000])m15(part,hole,block,casetop); // U3 (back)
};
module C12(){translate([-0.450000,-4.100000,1.600000])rotate([0,0,90.000000])children();}
module part_C12(part=true,hole=false,block=false)
{
translate([-0.450000,-4.100000,1.600000])rotate([0,0,90.000000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module C2(){translate([24.900000,-3.400000,1.600000])children();}
module part_C2(part=true,hole=false,block=false)
{
translate([24.900000,-3.400000,1.600000])m10(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module C9(){translate([25.500000,-4.950000,1.600000])children();}
module part_C9(part=true,hole=false,block=false)
{
translate([25.500000,-4.950000,1.600000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module U4(){translate([-19.500000,-8.000000,1.600000])rotate([0,0,180.000000])children();}
module part_U4(part=true,hole=false,block=false)
{
translate([-19.500000,-8.000000,1.600000])rotate([0,0,180.000000])m5(part,hole,block,casetop); // U5 (back)
};
module R1(){translate([26.900000,-5.800000,1.600000])rotate([0,0,90.000000])children();}
module part_R1(part=true,hole=false,block=false)
{
translate([26.900000,-5.800000,1.600000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module R8(){translate([26.000000,5.200000,1.600000])children();}
module part_R8(part=true,hole=false,block=false)
{
translate([26.000000,5.200000,1.600000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module C4(){translate([24.900000,7.375000,1.600000])rotate([0,0,-90.000000])children();}
module part_C4(part=true,hole=false,block=false)
{
translate([24.900000,7.375000,1.600000])rotate([0,0,-90.000000])m8(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module J3(){translate([-24.330000,6.500000,1.600000])rotate([0,0,-90.000000])children();}
module part_J3(part=true,hole=false,block=false)
{
translate([-24.330000,6.500000,1.600000])rotate([0,0,-90.000000])m16(part,hole,block,casetop); // J3 (back)
};
// Parts to go on PCB (top)
module parts_top(part=false,hole=false,block=false){
part_J4(part,hole,block);
part_R14(part,hole,block);
part_C1(part,hole,block);
part_D10(part,hole,block);
part_D14(part,hole,block);
part_U5(part,hole,block);
part_R11(part,hole,block);
part_R10(part,hole,block);
part_SW1(part,hole,block);
part_V3(part,hole,block);
part_D1(part,hole,block);
part_C35(part,hole,block);
part_R4(part,hole,block);
part_D4(part,hole,block);
part_R3(part,hole,block);
part_C13(part,hole,block);
part_R6(part,hole,block);
part_R9(part,hole,block);
part_C10(part,hole,block);
part_J2(part,hole,block);
part_R7(part,hole,block);
part_D6(part,hole,block);
part_D7(part,hole,block);
part_V2(part,hole,block);
part_C8(part,hole,block);
part_R15(part,hole,block);
part_C16(part,hole,block);
part_D12(part,hole,block);
part_PCB1(part,hole,block);
part_C14(part,hole,block);
part_D11(part,hole,block);
part_D2(part,hole,block);
part_D5(part,hole,block);
part_C3(part,hole,block);
part_J5(part,hole,block);
part_D9(part,hole,block);
part_R12(part,hole,block);
part_D8(part,hole,block);
part_C11(part,hole,block);
part_C17(part,hole,block);
part_D3(part,hole,block);
part_C5(part,hole,block);
part_R2(part,hole,block);
part_C6(part,hole,block);
part_U2(part,hole,block);
part_R13(part,hole,block);
part_C7(part,hole,block);
part_C15(part,hole,block);
part_U1(part,hole,block);
part_D13(part,hole,block);
part_R5(part,hole,block);
part_L1(part,hole,block);
part_U3(part,hole,block);
part_C12(part,hole,block);
part_C2(part,hole,block);
part_C9(part,hole,block);
part_U4(part,hole,block);
part_R1(part,hole,block);
part_R8(part,hole,block);
part_C4(part,hole,block);
part_J3(part,hole,block);
}

parts_top=26;
module J1(){translate([16.000000,2.000000,0.000000])rotate([0,0,180.000000])rotate([180,0,0])children();}
module part_J1(part=true,hole=false,block=false)
{
};
// Parts to go on PCB (bottom)
module parts_bottom(part=false,hole=false,block=false){
part_J1(part,hole,block);
}

parts_bottom=0;
module b(cx,cy,z,w,l,h){translate([cx-w/2,cy-l/2,z])cube([w,l,h]);}
module m0(part=false,hole=false,block=false,height)
{ // RevK:USB-C-Socket-H CSP-USC16-TR
// USB connector
rotate([-90,0,0])translate([0,1.9,0])
{
	if(part)
	{
        b(0,1.57,0,7,1.14,0.2); // Pads

		translate([0,1.76-7.55,1.63])
		rotate([-90,0,0])
		hull()
		{
			translate([(8.94-3.26)/2,0,0])
			cylinder(d=3.26,h=7.55,$fn=24);
			translate([-(8.94-3.26)/2,0,0])
			cylinder(d=3.26,h=7.55,$fn=24);
		}
		translate([-8.94/2,0.99-1.1/2,0])cube([8.94,1.1,1.6301]);
		translate([-8.94/2,-3.2-1.6/2,0])cube([8.94,1.6,1.6301]);
	}
	if(hole)
		translate([0,-5.79,1.63])
		rotate([-90,0,0])
	{
		// Plug
		hull()
		{
			translate([(8.34-2.5)/2,0,-23+1])
			cylinder(d=2.5,h=23,$fn=24);
			translate([-(8.34-2.5)/2,0,-23+1])
			cylinder(d=2.5,h=23,$fn=24);
		}
		hull()
		{
            translate([(12-7)/2,0,-21-1])
			cylinder(d=7,h=21,$fn=24);
            translate([-(12-7)/2,0,-21-1])
			cylinder(d=7,h=21,$fn=24);
		}
		translate([0,0,-100-10])
			cylinder(d=5,h=100,$fn=24);
	}
}
}

module m1(part=false,hole=false,block=false,height)
{ // RevK:R_0201 R_0201_0603Metric
// 0402 Resistor
if(part)
{
	b(0,0,0,1.1,0.4,0.2); // Pad size
	b(0,0,0,0.6,0.3,0.3); // Chip
}
}

module m2(part=false,hole=false,block=false,height)
{ // RevK:C_0805 C_0805_2012Metric
// 0805 Capacitor
if(part)
{
	b(0,0,0,2,1.2,1); // Chip
	b(0,0,0,2,1.45,0.2); // Pad size
}
}

module m3(part=false,hole=false,block=false,height)
{ // D10
// 1.6x1.5mm LED
if(part)
{
        b(0,0,0,1.6+0.2,1.5+0.2,.6+0.2);
}
if(hole)
{
        hull()
        {
                b(0,0,.6+0.2,1.6+0.2,1.5+0.2,1);
                translate([0,0,height])cylinder(d=1.001,h=0.001,$fn=16);
        }
}
if(block)
{
        hull()
        {
                b(0,0,.6+0.2,3.6+0.2,3.6+0.2,1);
                translate([0,0,height])cylinder(d=2,h=1,$fn=16);
        }
}
}

module m4(part=false,hole=false,block=false,height)
{ // D14
// DFN1006-2L
if(part)
{
	b(0,0,0,1.0,0.6,0.45); // Chip
}
}

module m5(part=false,hole=false,block=false,height)
{ // U5
if(part)
{
	b(0,0,0,0.78,0.78,0.5);
}
}

module m6(part=false,hole=false,block=false,height)
{ // SW1
// Push switch
if(part)
{
 b(0,0,0,5,9,1);	// Pads
}
if(hole)
{
 b(0,0,0,6,6,height+1);
 hull()
 {
 	b(0,0,5,6,6,1);
	translate([0,0,7.5])cylinder(d=13,h=10);
 }
}
if(block)
{
 b(0,0,0,8,8,height);
 hull()
 {
 	b(0,0,4,8,8,1);
	translate([0,0,6.5])cylinder(d=15,h=10);
 }
}
}

module m7(part=false,hole=false,block=false,height)
{ // C35
if(part)
{
	b(0,0,0,3.5,2.8,1.9);
}
}

module m8(part=false,hole=false,block=false,height)
{ // RevK:C_0201 C_0201_0603Metric
// 0402 Capacitor
if(part)
{
        b(0,0,0,1.1,0.4,0.2); // Pad size
        b(0,0,0,0.6,0.3,0.3); // Chip
}
}

module m9(part=false,hole=false,block=false,height)
{ // J2
if(part)
{
	b(0,0.5,0,12.2,13,1);
	for(x=[-6,6])translate([x,0,0])cylinder(d=3,h=5);
}
if(hole)
{
	b(0,0.5,0,12.2,13,14.5);
	b(0,-6-10,1,9.6,20,12);
	for(x=[-6,6])translate([x,0,-3.2])cylinder(d=3,h=3.2);
	for(x=[-2.55,-0.51,1.53])translate([x,2.3,-3.2])cylinder(d1=0.9,d2=1.5,h=3.2-pcbthickness);
	for(x=[-1.53,0.51,2.55])translate([x,4.84,-3.2])cylinder(d1=0.9,d2=1.5,h=3.2-pcbthickness);
}
}

module m10(part=false,hole=false,block=false,height)
{ // RevK:C_0402 C_0402_1005Metric
// 0402 Capacitor
if(part)
{
	b(0,0,0,1.0,0.5,1); // Chip
	b(0,0,0,1.5,0.65,0.2); // Pad size
}
}

module m11(part=false,hole=false,block=false,height)
{ // J5
if(part)
{
	b(0,0.1,0,14.85,14.5,2); // Main case
	b(-7.75,6.85,0,1.2,1.5,0.2); // Tab
	b(-7.75,-2.75,0,1.2,2.2,0.2); // Tab
	b(7.75,-2.75,0,1.2,2.2,0.2); // Tab
	for(i=[0:8])b(2.25-i*1.1,7.85,0,0.7,1.6,0.4); // Pins
	b(-0.95,-2.15,0.25,11,15,1.5);	// Card (inserted)
}
if(hole)
{
    hull()
    {
        b(-0.95,-8.47-0.5,0.25,11,1,1.5);	// Card
        b(-0.95,-8.47-0.5-10,0.25-10,11+20,1,1.5+20);	// Card
    }
}
}

module m12(part=false,hole=false,block=false,height)
{ // U2
// SOT-563
if(part)
{
	b(0,0,0,1.3,1.7,1); // Part
	b(0,0,0,1.35,2.1,0.2); // Pads
}
}

module m13(part=false,hole=false,block=false,height)
{ // U1
// ESP32-S3-MINI-1
translate([-15.4/2,-15.45/2,0])
{
	if(part)
	{
		cube([15.4,20.5,0.8]);
		translate([0.7,0.5,0])cube([14,13.55,2.4]);
		cube([15.4,20.5,0.8]);
	}
}
}

module m14(part=false,hole=false,block=false,height)
{ // L1
// 5x5x4 Inductor
if(part)
{
	b(0,0,0,5,5,4.3);
}
}

module m15(part=false,hole=false,block=false,height)
{ // U3
if(part)
{
	cube([4,4,1],center=true);
}
}

module m16(part=false,hole=false,block=false,height)
{ // J3
if(part)
{
	b(0,-3.17,0,13.1,14.3,1);
	b(0,-3.17-6.65,0,14.5,1,1);
}
if(hole)
{
	b(0,-3.17,0,13.1,14.3,8);
	b(0,-3.17-6.65,0,14.5,1,8);
	b(0,-3.17-6.65-10,1.5,12,20,5);
	b(0,-3.17-6.65-12,-1,20,20,10);
	for(x=[-6.6,6.6])translate([x,0,-3.2])cylinder(d=3,h=3.2);
	for(x=[-3.5,-1,1,3.5])translate([x,3.01,-3.2])cylinder(d1=0.9,d2=1.5,h=3.2-pcbthickness);
}
}

// Generate PCB casework

height=casebottom+pcbthickness+casetop;
$fn=48;

module pyramid()
{ // A pyramid
 polyhedron(points=[[0,0,0],[-height,-height,height],[-height,height,height],[height,height,height],[height,-height,height]],faces=[[0,1,2],[0,2,3],[0,3,4],[0,4,1],[4,3,2,1]]);
}


module pcb_hulled(h=pcbthickness,r=0)
{ // PCB shape for case
	if(useredge)outline(h,r);
	else hull()outline(h,r);
}

module solid_case(d=0)
{ // The case wall
	hull()
        {
                translate([0,0,-casebottom])pcb_hulled(height,casewall-edge);
                translate([0,0,edge-casebottom])pcb_hulled(height-edge*2,casewall);
        }
}

module preview()
{
	pcb();
	color("#0f0")parts_top(part=true);
	color("#0f0")parts_bottom(part=true);
	color("#f00")parts_top(hole=true);
	color("#f00")parts_bottom(hole=true);
	color("#00f8")parts_top(block=true);
	color("#00f8")parts_bottom(block=true);
}

module top_half(fit=0)
{
	difference()
	{
		translate([-casebottom-100,-casewall-100,pcbthickness+0.01]) cube([pcbwidth+casewall*2+200,pcblength+casewall*2+200,height]);
		translate([0,0,pcbthickness])
        	{
			snape=lip/5;
			snaph=(lip-snape*2)/3;
			if(lipt==1)rotate(lipa)hull()
			{
				translate([0,-pcblength,lip/2])cube([0.001,pcblength*2,0.001]);
				translate([-lip/2,-pcblength,0])cube([lip,pcblength*2,0.001]);
			} else if(lipt==2)for(a=[0,90,180,270])rotate(a+lipa)hull()
			{
				translate([0,-pcblength-pcbwidth,lip/2])cube([0.001,pcblength*2+pcbwidth*2,0.001]);
				translate([-lip/2,-pcblength-pcbwidth,0])cube([lip,pcblength*2+pcbwidth*2,0.001]);
			}
            		difference()
            		{
                		pcb_hulled(lip,casewall);
				if(snap)
                        	{
					hull()
					{
						pcb_hulled(0.1,casewall/2-snap/2+fit);
						translate([0,0,snape])pcb_hulled(snaph,casewall/2+snap/2+fit);
						translate([0,0,lip-snape-snaph])pcb_hulled(0.1,casewall/2-snap/2+fit);
					}
					translate([0,0,lip-snape-snaph])pcb_hulled(snaph,casewall/2-snap/2+fit);
					hull()
					{
						translate([0,0,lip-snape])pcb_hulled(0.1,casewall/2-snap/2+fit);
						translate([0,0,lip])pcb_hulled(0.1,casewall/2+snap/2+fit);
					}
                        	}
				else pcb_hulled(lip,casewall/2+fit);
				if(lipt==0)translate([-pcbwidth,-pcblength,0])cube([pcbwidth*2,pcblength*2,lip]);
				else if(lipt==1) rotate(lipa)translate([0,-pcblength,0])hull()
				{
					translate([lip/2,0,0])cube([pcbwidth,pcblength*2,lip]);
					translate([-lip/2,0,lip])cube([pcbwidth,pcblength*2,lip]);
				}
				else if(lipt==2)for(a=[0,180])rotate(a+lipa)hull()
                		{
                            		translate([lip/2,lip/2,0])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                            		translate([-lip/2,-lip/2,lip])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                		}
            		}
            		difference()
            		{
				if(snap)
                        	{
					hull()
					{
						translate([0,0,-0.1])pcb_hulled(0.1,casewall/2+snap/2-fit);
						translate([0,0,snape-0.1])pcb_hulled(0.1,casewall/2-snap/2-fit);
					}
					translate([0,0,snape])pcb_hulled(snaph,casewall/2-snap/2-fit);
					hull()
					{
						translate([0,0,snape+snaph])pcb_hulled(0.1,casewall/2-snap/2-fit);
						translate([0,0,lip-snape-snaph])pcb_hulled(snaph,casewall/2+snap/2-fit);
						translate([0,0,lip-0.1])pcb_hulled(0.1,casewall/2-snap/2-fit);
					}
                        	}
				else pcb_hulled(lip,casewall/2-fit);
				if(lipt==1)rotate(lipa+180)translate([0,-pcblength,0])hull()
				{
					translate([lip/2,0,0])cube([pcbwidth,pcblength*2,lip+0.1]);
					translate([-lip/2,0,lip])cube([pcbwidth,pcblength*2,lip+0.1]);
				}
				else if(lipt==2)for(a=[90,270])rotate(a+lipa)hull()
                		{
                            		translate([lip/2,lip/2,0])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                            		translate([-lip/2,-lip/2,lip])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                		}
			}
            	}
		minkowski()
                {
                	union()
                	{
                		parts_top(part=true,hole=true);
                		parts_bottom(part=true,hole=true);
                	}
                	translate([-0.01,-0.01,-height])cube([0.02,0.02,height]);
                }
        }
	minkowski()
        {
        	union()
                {
                	parts_top(part=true,hole=true);
                	parts_bottom(part=true,hole=true);
                }
                translate([-0.01,-0.01,0])cube([0.02,0.02,height]);
        }
}

module case_wall()
{
	difference()
	{
		solid_case();
		translate([0,0,-height])pcb_hulled(height*2,0.02);
	}
}

module top_side_hole()
{
	difference()
	{
		intersection()
		{
			parts_top(hole=true);
			case_wall();
		}
		translate([0,0,-casebottom])pcb_hulled(height,casewall);
	}
}

module bottom_side_hole()
{
	difference()
	{
		intersection()
		{
			parts_bottom(hole=true);
			case_wall();
		}
		translate([0,0,edge-casebottom])pcb_hulled(height-edge*2,casewall);
	}
}

module parts_space()
{
	minkowski()
	{
		union()
		{
			parts_top(part=true,hole=true);
			parts_bottom(part=true,hole=true);
		}
		sphere(r=margin,$fn=6);
	}
}

module top_cut(fit=0)
{
	difference()
	{
		top_half(fit);
		if(parts_top)difference()
		{
			minkowski()
			{ // Penetrating side holes
				top_side_hole();
				rotate([180,0,0])
				pyramid();
			}
			minkowski()
			{
				top_side_hole();
				rotate([0,0,45])cylinder(r=margin,h=height,$fn=4);
			}
		}
	}
	if(parts_bottom)difference()
	{
		minkowski()
		{ // Penetrating side holes
			bottom_side_hole();
			pyramid();
		}
			minkowski()
			{
				bottom_side_hole();
				rotate([0,0,45])translate([0,0,-height])cylinder(r=margin,h=height,$fn=4);
			}
	}
}

module bottom_cut()
{
	difference()
	{
		 translate([-casebottom-50,-casewall-50,-height]) cube([pcbwidth+casewall*2+100,pcblength+casewall*2+100,height*2]);
		 top_cut(-fit);
	}
}

module top_body()
{
	difference()
	{
		intersection()
		{
			solid_case();
			pcb_hulled(casetop+pcbthickness,0.03);
		}
		if(parts_top||topthickness)minkowski()
		{
			union()
			{
				if(nohull)parts_top(part=true);
				else hull(){parts_top(part=true);pcb_hulled();}
				if(topthickness)pcb_hulled(casetop+pcbthickness-topthickness,0);
			}
			translate([0,0,margin-height])cylinder(r=margin,h=height,$fn=8);
		}
	}
	intersection()
	{
		pcb_hulled(casetop+pcbthickness,0.03);
		union()
		{
			parts_top(block=true);
			parts_bottom(block=true);
		}
	}
}

module top_edge()
{
	intersection()
	{
		case_wall();
		top_cut();
	}
}

module top_pos()
{ // Position for plotting bottom
	translate([0,0,pcbthickness+casetop])rotate([180,0,0])children();
}

module pcb_pos()
{	// Position PCB relative to base 
		translate([0,0,pcbthickness-height])children();
}

module top()
{
	top_pos()difference()
	{
		union()
		{
			top_body();
			top_edge();
		}
		parts_space();
		pcb_pos()pcb(height,r=margin);
	}
}

module bottom_body()
{ // Position for plotting top
	difference()
	{
		intersection()
		{
			solid_case();
			translate([0,0,-casebottom])pcb_hulled(casebottom+pcbthickness,0.03);
		}
		if(parts_bottom||bottomthickness)minkowski()
		{
			union()
			{
				if(nohull)parts_bottom(part=true);
				else hull()parts_bottom(part=true);
				if(bottomthickness)translate([0,0,bottomthickness-casebottom])pcb_hulled(casebottom+pcbthickness-bottomthickness,0);
			}
			translate([0,0,-margin])cylinder(r=margin,h=height,$fn=8);
		}
	}
	intersection()
	{
		translate([0,0,-casebottom])pcb_hulled(casebottom+pcbthickness,0.03);
		union()
		{
			parts_top(block=true);
			parts_bottom(block=true);
		}
	}
}

module bottom_edge()
{
	intersection()
	{
		case_wall();
		bottom_cut();
	}
}

module bottom_pos()
{
	translate([0,0,casebottom])children();
}

module bottom()
{
	bottom_pos()difference()
	{
		union()
		{
        		bottom_body();
        		bottom_edge();
		}
		parts_space();
		pcb(height,r=margin);
	}
}

module datecode()
{
	minkowski()
	{
		translate([datex,datey,-1])rotate(datea)scale([-1,1])linear_extrude(1)text(date,size=dateh,halign="center",valign="center",font=datef);
		cylinder(d1=datet,d2=0,h=datet,$fn=6);
	}
}
difference(){bottom();datecode();}