from fpdf import FPDF

class PDF(FPDF):
    def header(self):
        self.set_font('Helvetica', 'B', 15)
        self.cell(0, 10, 'GPS and Astronomical Commands Reference', 0, 1, 'C')
        self.ln(10)

    def footer(self):
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 8)
        self.cell(0, 10, f'Page {self.page_no()}', 0, 0, 'C')

    def chapter_title(self, num, label):
        self.set_font('Helvetica', 'B', 12)
        self.cell(0, 10, f'{num} {label}', 0, 1, 'L')
        self.ln(2)

    def chapter_body(self, body):
        self.set_font('Helvetica', '', 11)
        self.multi_cell(0, 5, body)
        self.ln()

    def code_block(self, code):
        self.set_font('Courier', '', 10)
        self.set_fill_color(240, 240, 240)
        self.multi_cell(0, 5, code, fill=True)
        self.ln()

pdf = PDF()
pdf.add_page()

# 1. Overview
pdf.chapter_title('1.', 'Overview')
pdf.chapter_body(
    "The functionality is split into three commands:\n\n"
    "- STAR: Calculates the position of a celestial object using the current time and location from the connected GPS module.\n"
    "- LOCATION: Sets a manual location (latitude/longitude) and time for use with the ASTRO command.\n"
    "- ASTRO: Calculates the position of a celestial object using the manual time and location set by the LOCATION command.\n\n"
    "Both STAR and ASTRO share the same syntax for specifying the target object and output variables."
)

# 2. Command Syntax
pdf.chapter_title('2.', 'Command Syntax')

pdf.set_font('Helvetica', 'B', 11)
pdf.cell(0, 10, '2.1 The STAR Command', 0, 1)
pdf.set_font('Helvetica', '', 11)
pdf.multi_cell(0, 5, "Uses live GPS data. Requires a valid GPS fix.")
pdf.ln(2)

pdf.set_font('Helvetica', 'B', 10)
pdf.cell(0, 8, 'Syntax 1: Named Object', 0, 1)
pdf.code_block("STAR object alt, az [, ra_out, dec_out]")
pdf.set_font('Helvetica', '', 11)
pdf.multi_cell(0, 5, 
    "- object: An unquoted token specifying the celestial body (e.g., MOON, JUPITER, SIRIUS). See Appendix A for the full catalog.\n"
    "- alt: Variable to store the calculated Altitude (degrees above horizon).\n"
    "- az: Variable to store the calculated Azimuth (degrees, 0 = North, 90 = East).\n"
    "- ra_out (Optional): Variable to store the object's current Right Ascension (hours).\n"
    "- dec_out (Optional): Variable to store the object's current Declination (degrees)."
)
pdf.ln(2)

pdf.set_font('Helvetica', 'B', 10)
pdf.cell(0, 8, 'Syntax 2: Manual Coordinates', 0, 1)
pdf.code_block("STAR alt, az, ra, dec [, pm_ra, pm_dec] [, ra_out, dec_out]")
pdf.set_font('Helvetica', '', 11)
pdf.multi_cell(0, 5, 
    "- alt, az: Variables to store the results.\n"
    "- ra: Right Ascension of the target in J2000.0 epoch (hours).\n"
    "- dec: Declination of the target in J2000.0 epoch (degrees).\n"
    "- pm_ra (Optional): Proper motion in RA (arcseconds/year). Default is 0.\n"
    "- pm_dec (Optional): Proper motion in Dec (arcseconds/year). Default is 0.\n"
    "- ra_out, dec_out (Optional): Variables to store the coordinates after precession to the current epoch."
)
pdf.ln(5)

pdf.set_font('Helvetica', 'B', 11)
pdf.cell(0, 10, '2.2 The LOCATION Command', 0, 1)
pdf.set_font('Helvetica', '', 11)
pdf.multi_cell(0, 5, "Sets the context for the ASTRO command.")
pdf.ln(2)

pdf.set_font('Helvetica', 'B', 10)
pdf.cell(0, 8, 'Syntax', 0, 1)
pdf.code_block('LOCATION date$, lat, long [, sidereal_out]')
pdf.set_font('Helvetica', '', 11)
pdf.multi_cell(0, 5, 
    "- date$: A string containing the date and time in the format \"dd/mm/yyyy hh:mm:ss\". The separators can be -, /, :, or space.\n"
    "- lat: Latitude in degrees (negative for South).\n"
    "- long: Longitude in degrees (negative for West).\n"
    "- sidereal_out (Optional): Variable to store the calculated Local Sidereal Time (hours)."
)
pdf.ln(2)
pdf.set_font('Helvetica', 'B', 10)
pdf.cell(0, 8, 'Example', 0, 1)
pdf.code_block('LOCATION "25/12/2025 22:30:00", -33.86, 151.21, lst')
pdf.ln(5)

pdf.set_font('Helvetica', 'B', 11)
pdf.cell(0, 10, '2.3 The ASTRO Command', 0, 1)
pdf.set_font('Helvetica', '', 11)
pdf.multi_cell(0, 5, "Identical to STAR but uses the context set by LOCATION instead of the GPS.")
pdf.ln(2)
pdf.set_font('Helvetica', 'B', 10)
pdf.cell(0, 8, 'Syntax', 0, 1)
pdf.code_block(
    "ASTRO object alt, az [, ra_out, dec_out]\n"
    "ASTRO alt, az, ra, dec [, pm_ra, pm_dec] [, ra_out, dec_out]"
)
pdf.ln(5)

# 3. Implementation Details & Math
pdf.chapter_title('3.', 'Implementation Details & Math')
pdf.chapter_body(
    "The implementation performs high-precision astronomical calculations suitable for telescope pointing or navigation.\n\n"
    "3.1 Time Systems\n"
    "- Julian Centuries (T): Time is converted to Julian Centuries from the J2000.0 epoch (2000 Jan 1.5 TT).\n"
    "- Sidereal Time: Local Sidereal Time (LST) is calculated using a polynomial approximation for Greenwich Mean Sidereal Time (GMST) and the observer's longitude.\n\n"
    "3.2 Planetary Calculations\n"
    "- Planets: Uses simplified VSOP87-based elements with perturbations for Mercury, Venus, Mars, Jupiter, Saturn, Uranus, and Neptune.\n"
    "- Moon: Uses a truncated ELP-2000/82 analytical series to calculate the Moon's position, including significant periodic terms for longitude, latitude, and distance.\n"
    "- Topocentric Correction: For the Moon and planets, the calculation corrects for the observer's position on the Earth's surface (parallax), which is critical for nearby bodies like the Moon.\n\n"
    "3.3 Stellar Calculations\n"
    "- Catalog: Contains J2000.0 coordinates and proper motion data for bright stars and deep sky objects.\n"
    "- Precession: Coordinates are precessed from the J2000.0 epoch to the current date using the rigorous method described in Meeus (Chapter 21).\n"
    "- Proper Motion: Applied based on the years elapsed since J2000.0.\n\n"
    "3.4 Coordinate Conversion\n"
    "- Equatorial to Horizontal: Converts Right Ascension and Declination to Altitude and Azimuth using standard spherical trigonometry.\n"
    "- Refraction: Atmospheric refraction is applied to the Altitude using the standard formula (Meeus Chapter 16), corrected for standard atmospheric pressure and temperature."
)

# Appendix A
pdf.add_page()
pdf.chapter_title('Appendix A:', 'Celestial Catalog')
pdf.chapter_body("The following objects are recognized by name in the STAR and ASTRO commands.")

pdf.set_font('Helvetica', 'B', 11)
pdf.cell(0, 10, 'Solar System Bodies:', 0, 1)
pdf.set_font('Helvetica', '', 11)
pdf.multi_cell(0, 5, "SUN, MOON, MERCURY, VENUS, MARS, JUPITER, SATURN, URANUS, NEPTUNE")
pdf.ln(5)

pdf.set_font('Helvetica', 'B', 11)
pdf.cell(0, 10, 'Stars and Deep Sky Objects:', 0, 1)
pdf.set_font('Courier', '', 9)

stars = [
    "Achernar", "Acrux", "Alcyone",
    "Aldebaran", "Algenib", "Algieba",
    "Algol", "Alhajoth", "Alhena",
    "Almaak", "Alnair", "Alnilam",
    "Alnitak", "Alphard", "Alpheratz",
    "Alpherg", "Alrescha", "Alsephina",
    "Alshain", "Altair", "Aludra",
    "Andromeda Galaxy", "Antares", "Arcturus",
    "Aspidiske", "Bellatrix", "Betelgeuse",
    "Bodes Galaxy", "Canopus", "Capella",
    "Caph", "Castor", "Cigar Galaxy",
    "Deneb", "Denebola", "Dubhe",
    "Elnath", "Eltanin", "Enif",
    "Fomalhaut", "Gacrux", "Hadar",
    "Homam", "Kaus Australis", "Kochab",
    "Kornephoros", "Large Magellanic Cloud", "Lesath",
    "Markab", "Menkalinan", "Mimosa",
    "Mintaka", "Mirfak", "Nunki",
    "Peacock", "Polaris", "Pollux",
    "Procyon", "Rasalgethi", "Rasalhague",
    "Regulus", "Rigel", "Rigil Kent",
    "Ruchbah", "Sabik", "Sadalmelik",
    "Sadalsuud", "Sadr", "Saiph",
    "Scheat", "Shaula", "Shedir",
    "Sirius", "Small Magellanic Cloud", "Sombrero Galaxy",
    "Spica", "Suhail", "Tarazed",
    "Triangulum Galaxy", "Vega", "Whirlpool Galaxy",
    "Zubenelgenubi", "Zubeneschamali"
]

# Simple 3-column layout
col_width = pdf.w / 3.5
for i in range(0, len(stars), 3):
    row = stars[i:i+3]
    for star in row:
        pdf.cell(col_width, 5, star, 0, 0)
    pdf.ln()

pdf.output("GPS_Astro_Reference.pdf")
