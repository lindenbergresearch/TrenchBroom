import os
import struct
from PIL import Image  # Only required if export_pngs=True

VERSION_ID = 29
VERSION_HEADER_LENGTH = 4


def export_bsp_textures(bsp_path, output_dir, export_pngs=True, create_wad=True, wad_filename="textures.wad"):
    """
    Parses a Quake 1 BSP file, extracts embedded textures,
    and can save them as individual PNG files and/or pack them into a Quake WAD2 archive.
    """
    if not os.path.exists(output_dir) and (export_pngs or create_wad):
        os.makedirs(output_dir)

    # Standard Quake 1 8-bit palette (256 RGB colors)
    quake_palette = [
        0, 0, 0, 15, 15, 15, 31, 31, 31, 47, 47, 47, 63, 63, 63, 75, 75, 75, 91, 91, 91, 107, 107, 107, 123, 123, 123,
        139, 139, 139, 155, 155, 155, 171, 171, 171, 187, 187, 187, 203, 203, 203, 219, 219, 219, 235, 235, 235,
        11, 11, 95, 27, 27, 107, 43, 43, 119, 59, 59, 131, 75, 75, 143, 91, 91, 155, 107, 107, 167, 123, 123, 179, 135,
        135, 191, 147, 147, 203, 159, 159, 215, 171, 171, 227, 183, 183, 239, 195, 195, 251, 207, 207, 255,
        11, 27, 95, 27, 43, 107, 43, 59, 119, 59, 75, 131, 75, 91, 143, 91, 107, 155, 107, 123, 167, 123, 139, 179, 135,
        155, 191, 147, 171, 203, 159, 187, 215, 171, 203, 227, 183, 219, 239, 195, 235, 251, 203, 247, 255,
        11, 95, 11, 27, 107, 27, 43, 119, 43, 59, 131, 59, 75, 143, 75, 91, 155, 91, 107, 167, 107, 123, 179, 123, 135,
        191, 135, 147, 203, 147, 159, 215, 159, 171, 227, 171, 183, 239, 183, 195, 251, 195, 207, 255, 207,
        95, 11, 11, 107, 27, 27, 119, 43, 43, 131, 59, 59, 143, 75, 75, 155, 91, 91, 167, 107, 107, 179, 123, 123, 191,
        135, 135, 203, 147, 147, 215, 159, 159, 227, 171, 171, 239, 183, 183, 251, 195, 195, 255, 207, 207,
        95, 11, 95, 107, 27, 107, 119, 43, 119, 131, 59, 131, 143, 75, 143, 155, 91, 155, 167, 107, 167, 179, 123, 179,
        191, 135, 191, 203, 147, 203, 215, 159, 215, 227, 171, 227, 239, 183, 239, 251, 195, 251, 255, 207,
        95, 95, 11, 107, 107, 27, 119, 119, 43, 131, 131, 59, 143, 143, 75, 155, 155, 91, 167, 167, 107, 179, 179, 123,
        191, 191, 135, 203, 203, 147, 215, 215, 159, 227, 227, 171, 239, 239, 183, 251, 251, 195, 255, 255, 207,
        15, 23, 39, 23, 31, 51, 31, 43, 63, 39, 51, 75, 47, 63, 87, 55, 75, 99, 63, 83, 111, 71, 95, 123, 79, 103, 135,
        87, 115, 147, 95, 123, 159, 103, 135, 171, 111, 143, 183, 119, 155, 195, 127, 163, 207, 135, 175, 219,
        31, 15, 11, 43, 23, 19, 55, 35, 27, 67, 43, 35, 79, 55, 43, 91, 63, 51, 103, 75, 59, 115, 83, 67, 127, 95, 75,
        139, 103, 83, 151, 115, 91, 163, 123, 99, 175, 135, 107, 187, 143, 115, 199, 155, 123, 211, 163, 131,
        71, 55, 43, 83, 67, 51, 95, 75, 59, 107, 87, 67, 119, 95, 75, 131, 107, 83, 143, 115, 91, 155, 127, 99, 167,
        135, 107, 179, 147, 115, 191, 155, 123, 203, 167, 131, 215, 175, 139, 227, 187, 147, 239, 195, 155, 251, 207,
        167,
        11, 95, 95, 27, 107, 107, 43, 119, 119, 59, 131, 131, 75, 143, 143, 91, 155, 155, 107, 167, 167, 123, 179, 179,
        135, 191, 191, 147, 203, 203, 159, 215, 215, 171, 227, 227, 183, 239, 239, 195, 251, 251, 207, 255, 255,
        15, 39, 15, 23, 51, 23, 31, 63, 31, 39, 75, 39, 47, 87, 47, 55, 99, 55, 63, 111, 63, 71, 123, 71, 79, 135, 79,
        87, 147, 87, 95, 159, 95, 103, 171, 103, 111, 183, 111, 119, 195, 119, 127, 207, 127, 135, 219, 135,
        39, 15, 15, 51, 23, 23, 63, 31, 31, 75, 39, 39, 87, 47, 47, 99, 55, 55, 111, 63, 63, 123, 71, 71, 135, 79, 79,
        147, 87, 87, 159, 95, 95, 171, 103, 103, 183, 111, 111, 195, 119, 119, 207, 127, 127, 219, 135, 135,
        15, 15, 15, 27, 27, 27, 39, 39, 39, 51, 51, 51, 63, 63, 63, 75, 75, 75, 87, 87, 87, 99, 99, 99, 111, 111, 111,
        123, 123, 123, 135, 135, 135, 147, 147, 147, 159, 159, 159, 171, 171, 171, 183, 183, 183, 195, 195, 195,
        255, 243, 27, 239, 223, 23, 223, 207, 19, 207, 191, 15, 191, 175, 11, 175, 159, 7, 159, 143, 0, 143, 127, 0,
        127, 111, 0, 111, 95, 0, 95, 79, 0, 79, 63, 0, 63, 47, 0, 47, 31, 0, 31, 15, 0, 15, 0, 0,
        0, 0, 255, 11, 11, 239, 23, 23, 223, 35, 35, 207, 47, 47, 191, 59, 59, 175, 71, 71, 159, 83, 83, 143, 95, 95,
        127, 107, 107, 111, 119, 119, 95, 131, 131, 79, 143, 143, 63, 155, 155, 47, 167, 167, 31, 179, 179, 15
    ]

    extracted_textures = []

    with open(bsp_path, 'rb') as f:
        # Check BSP version (Quake 1 standard is version 29)
        version = struct.unpack('<I', f.read(VERSION_HEADER_LENGTH))[0]
        if version != VERSION_ID:
            print(f"Warning: Unknown BSP version ({version}). Expected Quake 1 (v29).")

        # Jump to Lump 2 (Texture Lump directory entry: 4 bytes offset, 4 bytes size)
        f.seek(4 + 2 * 8)
        tex_offset, tex_size = struct.unpack('<II', f.read(8))

        if tex_size == 0:
            print("No textures found embedded inside this BSP file (uses external WAD only).")
            return

        # Go to the actual texture lump location
        f.seek(tex_offset)
        num_textures = struct.unpack('<I', f.read(4))[0]
        print(f"Total textures listed in BSP: {num_textures}")

        # Read the internal relative offsets for each texture
        tex_offsets = struct.unpack(f'<{num_textures}i', f.read(num_textures * 4))

        for offset in tex_offsets:
            if offset == -1:
                # Texture is external, skip it
                continue

            # Move to the start of this specific texture data structure (Miptex)
            f.seek(tex_offset + offset)

            # Read header elements to know how big the full texture payload is
            header_data = f.read(40)
            name_raw, width, height = struct.unpack('<16sII', header_data[:24])
            mip_offsets = struct.unpack('<4I', header_data[24:])

            # Decode and clean texture name
            name = name_raw.split(b'\x00')[0].decode('ascii', errors='ignore')
            clean_name = "".join([c if c.isalnum() or c in "._-" else "_" for c in name])

            # Calculate total binary size of the Miptex payload (Header + 4 Mipmaps data sizes)
            # Mip 0: w * h, Mip 1: (w/2)*(h/2), Mip 2: (w/4)*(h/4), Mip 3: (w/8)*(h/8)
            # Total pixel bytes = (w*h) * (1 + 1/4 + 1/16 + 1/64) = (w*h*85)/64
            mipmaps_total_bytes = (width * height * 85) // 64
            total_miptex_size = 40 + mipmaps_total_bytes

            # Read the entire raw Miptex data block directly from the file
            f.seek(tex_offset + offset)
            raw_miptex_data = f.read(total_miptex_size)

            # Store texture information for PNG/WAD export
            extracted_textures.append({
                'name': name,
                'clean_name': clean_name,
                'width': width,
                'height': height,
                'mip0_offset': mip_offsets[0],
                'raw_data': raw_miptex_data,
                'size': total_miptex_size
            })

    # Optional Step 1: Export to individual PNG files
    if export_pngs and extracted_textures:
        print("\n--- Exporting PNG images ---")
        for tex in extracted_textures:
            # Extract just the pixel data for Mipmap 0 (Full size)
            pixel_start = tex['mip0_offset']
            pixel_end = pixel_start + (tex['width'] * tex['height'])
            pixel_data = tex['raw_data'][pixel_start:pixel_end]

            # Generate PNG using Pillow
            img = Image.frombytes('P', (tex['width'], tex['height']), pixel_data)
            img.putpalette(quake_palette)

            png_path = os.path.join(output_dir, f"{tex['clean_name']}.png")
            img.save(png_path)
            print(f"Saved image: {png_path}")

    # Step 2: Compile all raw texture data into a standard Quake WAD2 archive
    if create_wad and extracted_textures:
        wad_path = os.path.join(output_dir, wad_filename)
        print(f"\n--- Creating Quake WAD2 Archive: {wad_path} ---")

        # WAD2 structure definitions
        # WAD Header: Magic (4B "WAD2"), NumDirs (4B int), DirOffset (4B int)
        wad_magic = b"WAD2"
        num_entries = len(extracted_textures)

        # Calculate exactly where the directory block will start
        # Header (12 bytes) + combined length of all raw texture payloads
        current_data_offset = 12
        directory_entries = []
        combined_texture_payload = b""

        for tex in extracted_textures:
            # Build WAD directory item data structure
            # Each entry is 32 bytes:
            # filepos (4B), disksize (4B), size (4B), type (1B), compression (1B), pad (2B), name (16B)
            filepos = current_data_offset
            disksize = tex['size']
            size = tex['size']
            entry_type = 0x43  # TYP_MIPTEX (0x43) is used for standard Quake textures
            compression = 0  # No compression
            pad = 0

            # Format name into fixed 16-byte buffer
            name_bytes = tex['name'].encode('ascii', errors='ignore')[:15]
            name_padded = name_bytes.ljust(16, b'\x00')

            # Pack individual directory entry structure
            dir_entry = struct.pack('<IIIbbh16s', filepos, disksize, size, entry_type, compression, pad, name_padded)
            directory_entries.append(dir_entry)

            # Append the raw text data payload and increment offset tracking
            combined_texture_payload += tex['raw_data']
            current_data_offset += tex['size']

        # Pack the final unified directory offset into the primary header block
        wad_header = struct.pack('<4sII', wad_magic, num_entries, current_data_offset)

        # Write out everything sequentially into the unified .wad file
        with open(wad_path, 'wb') as wad_file:
            wad_file.write(wad_header)
            wad_file.write(combined_texture_payload)
            for entry in directory_entries:
                wad_file.write(entry)

        print(f"WAD Archive successfuly created with {num_entries} textures!")


if __name__ == "__main__":
    # Configure your paths and options here
    bsp_file = "start.bsp"
    output_folder = "./extracted_content"

    try:
        export_bsp_textures(
            bsp_path=bsp_file,
            output_dir=output_folder,
            export_pngs=True,  # Set to False if you don't want PNGs
            create_wad=True,  # Set to True to generate the .wad package
            wad_filename="map_textures.wad"
        )
        print("\nAll tasks completed successfully!")
    except FileNotFoundError:
        print(f"Error: The file '{bsp_file}' was not found. Please verify the file path.")
