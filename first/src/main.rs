extern crate png;
extern crate rayon;

use rayon::prelude::*;
use std::sync::atomic::{AtomicU8, Ordering};
use std::time::{Instant, Duration};
use std::cmp;

fn to_atomic_u8(buf: & Vec<u8>) -> Vec<AtomicU8> {
    let mut res: Vec<AtomicU8> = Vec::with_capacity(buf.len());
    for &x in buf {
        res.push(AtomicU8::new(x));
    }
    res
}

fn cp_atomic_u8(buf: & Vec<AtomicU8>) -> Vec<AtomicU8> {
    let mut res: Vec<AtomicU8> = Vec::with_capacity(buf.len());
    for x in buf {
        let v: u8 = x.load(Ordering::Relaxed);
        res.push(AtomicU8::new(v));
    }
    res
}

fn replace_vals(dst: &mut Vec<u8>, src: & Vec<AtomicU8>) {
    for i in 1 .. src.len() {
        let v = src[i].load(Ordering::Relaxed);
        dst[i] = v;
    }
}

fn modify_seq(buf: &mut Vec<u8>, width: u32, height: u32, nc: u32, increment: u8) {
    // calculate
    let num_pixels: usize = (width * height) as usize;
    for pixel_index in 0 .. num_pixels {
        let i0 = pixel_index * nc as usize;
        for j in 0 .. nc-1 {
            let i = i0 + j as usize;
            let val : u8 = buf[i];
            let modified : u8 = val.saturating_add(increment);
            buf[i] = modified;
        }
    }
}


fn modify_block(buf_in: &mut Vec<u8>, width: u32, height: u32, nc: u32, increment: u8, block_size: u32) {
    let mut  num_width_blocks = width / block_size as u32;
    let rem_width_blocks = width % block_size as u32;
    let mut num_height_blocks = height / block_size as u32;
    let rem_height_blocks = height % block_size as u32;

    if rem_width_blocks > 0 {
        num_width_blocks += 1;
    }
    if rem_height_blocks > 0 {
        num_height_blocks += 1;
    }

    let coordinates: Vec<(u32, u32)> =
        (0..num_width_blocks)
        .flat_map(|x| (0..num_height_blocks)
        .map(move |y| (x, y)))
        .collect();

    let buf = to_atomic_u8(buf_in);

    coordinates.par_iter().for_each(|&(block_x, block_y)| {
        // Process each block
        let x0 = block_x * block_size;
        let y0 = block_y * block_size;
        let x1 = cmp::min(x0 + block_size, width);
        let y1 = cmp::min(y0 + block_size, height);
        //println!("Processing coordinate ({}, {} -> {}, {})", x0, y0, x1, y1);

        // calculate
        for y in y0 .. y1 {
            for x in x0 .. x1 {
                let index = (y * width + x) * nc;
                for j in 0 .. nc-1 {
                    let i : usize = (index + j) as usize;
                    let val : u8 = buf[i].load(Ordering::Relaxed);
                    let modified : u8 = val.saturating_add(increment);
                    buf[i].store(modified, Ordering::Relaxed);
                }
            }
        }
    });

    replace_vals( buf_in, &buf);
}

fn modify_batch(buf_in: &mut Vec<u8>, width: u32, height: u32, nc: u32, increment: u8, batch_size: u32) {
    let num_pixels = width * height;
    let mut num_pixel_chunks = num_pixels / batch_size;
    let rem_pixel_chunks = num_pixels % batch_size;
    if rem_pixel_chunks > 0 {
        num_pixel_chunks += 1;
    }
    let locations: Vec<u32> = (0..num_pixel_chunks).map(|x| x * batch_size).collect();
    //println!("{:.?}", locations);

    let par_buf = to_atomic_u8(buf_in);
    locations.par_iter().for_each(|&block_index| {
        // Process each block
        let x0 = block_index;
        let x1 = cmp::min(x0 + batch_size, num_pixels);

        for x in x0 .. x1 {
            let index = x * nc;
            for j in 0 .. nc-1 {
                let i : usize = (index + j) as usize;
                let val : u8 = par_buf[i].load(Ordering::Relaxed);
                let modified : u8 = val.saturating_add(increment);
                par_buf[i].store(modified, Ordering::Relaxed);
            }
        }
    });

    replace_vals( buf_in, &par_buf);
}

fn profile_sequential(num_samples: u32, buf_orig: &mut Vec<u8>, width: u32, height: u32, nc: u32, increment: u8) -> Vec<Duration> {
    let mut durations_vec: Vec<Duration> = Vec::new();

    for _i in 1..num_samples {
        // prepare for calculations
        let mut buf = buf_orig.clone();

        // start timer
        let t_start1 = Instant::now();

        // calculate
        let num_pixels: usize = (width * height) as usize;
        for pixel_index in 0 .. num_pixels {
            let i0 = pixel_index * nc as usize;
            for j in 0 .. nc-1 {
                let i = i0 + j as usize;
                let val : u8 = buf[i];
                let modified : u8 = val.saturating_add(increment);
                buf[i] = modified;
            }
        }

        // save result
        let d_t1 = t_start1.elapsed();    
        durations_vec.push(d_t1);
    }

    durations_vec
}

fn profile_parallel_batches(num_samples: u32, buf_orig: &mut Vec<AtomicU8>, width: u32, height: u32, nc: u32, batch_size: u32, increment: u8) -> Vec<Duration> {
    let mut durations_vec: Vec<Duration> = Vec::new();

    let num_pixels = width * height;
    let mut num_pixel_chunks = num_pixels / batch_size as u32;
    let rem_pixel_chunks = num_pixels % batch_size as u32;
    if rem_pixel_chunks > 0 {
        num_pixel_chunks += 1;
    }


    let locations: Vec<(u32, u32)> = (0..num_pixel_chunks)
            .map(|x| (x * batch_size, cmp::min((x+1) * batch_size, num_pixels)))
            .collect();
    //println!("{:.?}", locations);

    for _i in 1..num_samples {
        // prepare for calculations
        let buf = cp_atomic_u8(buf_orig);

        // start timer
        let t_start1 = Instant::now();

        locations.par_iter().for_each(|&(x0, x1)| {
            // Process each block
            for x in x0 .. x1 {
                let index = x * nc;
                for j in 0 .. nc-1 {
                    let i : usize = (index + j) as usize;
                    let val : u8 = buf[i].load(Ordering::Relaxed);
                    let modified : u8 = val.saturating_add(increment);
                    buf[i].store(modified, Ordering::Relaxed);
                }
            }
        });
    
        // store result
        let d_t1 = t_start1.elapsed();    
        durations_vec.push(d_t1);
    }

    durations_vec
}

fn profile_parallel_blocks(num_samples: u32, buf_orig: &mut Vec<AtomicU8>, width: u32, height: u32, nc: u32, block_size: u32, increment: u8) -> Vec<Duration> {
    let mut durations_vec: Vec<Duration> = Vec::new();

    let mut  num_width_blocks = width / block_size as u32;
    let rem_width_blocks = width % block_size as u32;
    let mut num_height_blocks = height / block_size as u32;
    let rem_height_blocks = height % block_size as u32;

    if rem_width_blocks > 0 {
        num_width_blocks += 1;
    }
    if rem_height_blocks > 0 {
        num_height_blocks += 1;
    }

    let coordinates: Vec<(u32, u32)> =
        (0..num_width_blocks)
        .flat_map(|x| (0..num_height_blocks)
        .map(move |y| (x, y)))
        .collect();

    for _i in 1..num_samples {
        // prepare for calculations
        let buf = cp_atomic_u8(buf_orig);

        // start timer
        let t_start1 = Instant::now();

        coordinates.par_iter().for_each(|&(block_x, block_y)| {
            // Process each block
            let x0 = block_x * block_size;
            let y0 = block_y * block_size;
            let x1 = cmp::min(x0 + block_size, width);
            let y1 = cmp::min(y0 + block_size, height);
            //println!("Processing coordinate ({}, {} -> {}, {})", x0, y0, x1, y1);

            // calculate
            for y in y0 .. y1 {
                for x in x0 .. x1 {
                    let index = (y * width + x) * nc;
                    for j in 0 .. nc-1 {
                        let i : usize = (index + j) as usize;
                        let val : u8 = buf[i].load(Ordering::Relaxed);
                        let modified : u8 = val.saturating_add(increment);
                        buf[i].store(modified, Ordering::Relaxed);
                    }
                }
            }
        });
    
        // store result
        let d_t1 = t_start1.elapsed();    
        durations_vec.push(d_t1);
    }

    durations_vec
}

fn save_png(filename: &std::path::Path, info: &png::OutputInfo, image_buffer: &Vec<u8>) {
    let file = std::fs::File::create(filename).unwrap();
    let ref mut buf_writer = std::io::BufWriter::new(file);
    let mut encoder  = png::Encoder::new(buf_writer, info.width, info.height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    encoder.set_source_gamma(png::ScaledFloat::from_scaled(45455));
    encoder.set_source_gamma(png::ScaledFloat::new(1.0/2.2));
    let source_chromacities = png::SourceChromaticities::new(
        (0.31270, 0.32900),
        (0.64000, 0.33000),
        (0.30000, 0.60000),
        (0.15000, 0.06000)
    );

    encoder.set_source_chromaticities(source_chromacities);
    let mut writer = encoder.write_header().unwrap();
    writer.write_image_data(&image_buffer).unwrap();
}

fn calc_stats(values: &Vec<Duration>) -> (Duration, Duration) {
    let sum: Duration = values.iter().sum();
    let avg = sum.as_nanos() / values.len() as u128;
    let avg_dur = Duration::from_nanos(avg as u64);

    let squared_diff_sum: f64 = values.iter().map(|&d| {
        let diff = d.as_nanos() as f64 - avg as f64;
        diff * diff
    }).sum();
    let stdval = (squared_diff_sum / values.len() as f64).sqrt();
    let stdval_dur = Duration::from_nanos(stdval as u64);

    (avg_dur, stdval_dur)
}


fn load_png(inpath: &std::path::Path) -> (Vec<u8>, png::OutputInfo) {
    let decoder = png::Decoder::new(std::fs::File::open(inpath).unwrap());
    let mut reader = decoder.read_info().unwrap();
    let mut buf: Vec<u8> = vec![0; reader.output_buffer_size()];
    let info: png::OutputInfo = reader.next_frame(&mut buf).unwrap();

    (buf, info)
}

fn main() {
    let filename_in = std::path::Path::new(r"pillars.png");
    let (mut buf, info) = load_png(filename_in);
    println!("Image: {:.?}, dimensions: ({}, {}), ColorType: {:.?}, BitDepth: {:.?}", filename_in, info.width, info.height, info.color_type, info.bit_depth);
    if info.color_type != png::ColorType::Rgba {
        println!("Only rgba images are supported in this program");
        std::process::exit(1);
    }

    let increment: u8 = 64;
    let block_size: u32 = 64;
    let batch_size: u32 = 64*64;
    let num_samples: u32 = 1000;

    let filename_seq = std::path::Path::new(r"out_seq.png");
    let filename_batch = std::path::Path::new(r"out_batch.png");
    let filename_block = std::path::Path::new(r"out_block.png");

    // This is just to store the results to make sure the trivial calculation is correct
    let mut buf_copy_seq = buf.clone();
    let mut buf_copy_batch = buf.clone();
    let mut buf_copy_block = buf.clone();
    modify_seq(&mut buf_copy_seq, info.width, info.height, 4, increment);
    modify_batch(&mut buf_copy_batch, info.width, info.height, 4, increment, batch_size);
    modify_block(&mut buf_copy_block, info.width, info.height, 4, increment, block_size);
    save_png(filename_seq, &info, &buf_copy_seq);
    save_png(filename_batch, &info, &buf_copy_batch);
    save_png(filename_block, &info, &buf_copy_block);
    println!("Stored seq, batch, block in: {:.?}, {:.?} and {:.?}", filename_seq, filename_batch, filename_block);

    let num_cores = rayon::current_num_threads();
    println!("Number of logical cores: {}", num_cores);
    println!("Number of samples: {}", num_samples);

    println!("Performing performance analysis on sequential code");
    println!("Performing performance analysis on parallel code with batch_size: {}", batch_size);
    println!("Performing performance analysis on parallel blocks with size: ({}, {})", block_size, block_size);

    let sequential_times = profile_sequential(num_samples, &mut buf, info.width, info.height, 4, increment);
    let mut buf_au8: Vec<AtomicU8> = to_atomic_u8(&buf);
    let parallel_batch = profile_parallel_batches(num_samples, &mut buf_au8, info.width, info.height, 4, batch_size, increment);
    let mut buf_au8: Vec<AtomicU8> = to_atomic_u8(&buf);
    let parallel_block = profile_parallel_blocks(num_samples, &mut buf_au8, info.width, info.height, 4, batch_size, increment);

    let (avg_seq, std_dev_seq)=calc_stats(&sequential_times);
    let (avg_batch, std_dev_batch)=calc_stats(&parallel_batch);
    let (avg_block, std_dev_block)=calc_stats(&parallel_block);

    println!("Sequential     time: {:.?}, standard deviation:  {:.?}", avg_seq, std_dev_seq);
    println!("Parallel Batch time: {:.?}, standard deviation:  {:.?}", avg_batch, std_dev_batch);
    println!("Parallel Block time: {:.?}, standard deviation:  {:.?}", avg_block, std_dev_block);
}
