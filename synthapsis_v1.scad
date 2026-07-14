box_lunghezza = 170; 
box_larghezza = 170;
box_altezza   = 100;     // 100
parete        = 1.75;

usb_l = 11;
usb_a = 7;
jack_r = 3.5;

$fn = 60;

module base_scatola() {
    difference() {
        
        cube([box_lunghezza, box_larghezza, box_altezza]);

        translate([parete, parete, parete])
            cube([
                box_lunghezza - (parete * 2),
                box_larghezza - (parete * 2),
                box_altezza
            ]);

        translate([
            10 + parete,
            -1,
            parete
        ])
            cube([usb_l, parete + 2, usb_a]);

        translate([
            box_lunghezza - 15 - parete,
            -1,
            parete + jack_r + 3
        ])
            rotate([0, 90, 90])
                cylinder(h = parete + 3, r = jack_r);
    }
}


altezza_labbro = 8;
spessore_labbro = 1.2;
tolleranza    = 0.2;
buco_diametro = 1.31;

module coperchio_flex() {
    display_w = 12;
    display_h = 3;
    
    potent_w = 15; //15
    potent_h = 5;  //5
    
    loop_btn_w = 32;
    loop_btn_h = 16;
    pause_btn_w = 25;
    pause_btn_h = 16;
    space_between = 13;
    
    note = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "-"];
    
    raggio_x = 60; // Horizontal stretch (wider)
    raggio_y = 50; // Vertical depth (shallower)
    centro_x = box_lunghezza / 2;
    centro_y = box_altezza - raggio_y + 20;

    difference() {
        cube([box_lunghezza, box_larghezza, parete]);
        
        translate([box_lunghezza / 2 - display_w / 2, box_larghezza / 2 - display_h / 2, -1])
            cube([display_w, display_h, parete + 2]);
        
        translate([50 - potent_w/2, box_larghezza - 50 - potent_h/2, -1])
        rotate([0, 0, -15])
            cube([potent_w, potent_h, parete + 2]);
        
        
        translate([box_lunghezza - 83, box_larghezza - 30, -1])
        rotate([0, 0, -15])
            difference() {
                cube([loop_btn_w + space_between + pause_btn_w, loop_btn_h, parete + 2]);
                
                translate([pause_btn_w, 0, 0])
                    cube([space_between, loop_btn_h, parete + 2]);
            };
        
        
        for (i = [0 : 12]) {
            angolo = 180 + (i * 15);
            
            // Calculate the radii for the inner holes
            raggio_x_in = raggio_x - 3.75;
            raggio_y_in = raggio_y - 3.75;
            
            // Calculate the radii for the outer holes
            raggio_x_out = raggio_x + 3.75;
            raggio_y_out = raggio_y + 3.75;
            
            // Calculate the radii for the text (placed further out)
            raggio_x_text = raggio_x + 10;
            raggio_y_text = raggio_y + 10;
            
            // Position for the inner hole
            pos_x_in = centro_x + (raggio_x_in * cos(angolo));
            pos_y_in = centro_y + (raggio_y_in * sin(angolo));
            
            // Position for the outer hole
            pos_x_out = centro_x + (raggio_x_out * cos(angolo));
            pos_y_out = centro_y + (raggio_y_out * sin(angolo));
            
            // Position for the text
            pos_x_text = centro_x + (raggio_x_text * cos(angolo));
            pos_y_text = centro_y + (raggio_y_text * sin(angolo));
            
            // Cut the inner hole
            translate([pos_x_in, pos_y_in, -1])
                cylinder(h = parete + 2, d = buco_diametro);
            
            // Cut the outer hole
            translate([pos_x_out, pos_y_out, -1])
                cylinder(h = parete + 2, d = buco_diametro);
            
            // Cut the partially hollow text, reversed and mirrored
            // Starts at -0.1 and extrudes 0.8 to create a 0.7mm engraving in the base
            translate([pos_x_text, pos_y_text, -0.1])
                // rotate([0, 0, angolo + 90])
                linear_extrude(height = 0.8)
                mirror([1, 0, 0])
                offset(r = 0.1)
                text(note[12 - i], size = 5.5, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
        }
    }

    translate([
        parete + tolleranza,
        parete + tolleranza,
        parete
    ])
    difference() {
        cube([
            box_lunghezza - (parete * 2) - (tolleranza * 2),
            box_larghezza - (parete * 2) - (tolleranza * 2),
            altezza_labbro
        ]);
        
        translate([spessore_labbro, spessore_labbro, -1])
            cube([
                box_lunghezza - (parete * 2) - (tolleranza * 2) - (spessore_labbro * 2),
                box_larghezza - (parete * 2) - (tolleranza * 2) - (spessore_labbro * 2),
                altezza_labbro + 2
            ]);
        
        translate([-5, box_larghezza / 4, -1])
            cube([box_lunghezza + 10, 2, altezza_labbro + 2]);
        translate([-5, (box_larghezza / 4) * 3, -1])
            cube([box_lunghezza + 10, 2, altezza_labbro + 2]);
        
        translate([box_lunghezza / 4, -5, -1])
            cube([2, box_larghezza + 10, altezza_labbro + 2]);
        translate([(box_lunghezza / 4) * 3, -5, -1])
            cube([2, box_larghezza + 10, altezza_labbro + 2]);
    }
}


module test_coperchio(foro_d) {
    test_lato = 15;
    
    difference() {
        cube([test_lato, test_lato, parete]);
        
        translate([test_lato / 2, test_lato / 2, -1])
            cylinder(h = parete + 2, d = foro_d);
    }
}

//translate([box_lunghezza + 10, 0, 0]) 
//    test_coperchio(1.31);

coperchio_flex();
