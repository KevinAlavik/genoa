# Genoa Physical Memory Manager (PMM)

## What’s It Made Of?
- **Bitmap**: A big array where each bit tracks a page. `0` = free, `1` = used. For a 1GB system, that’s about 32KB of bitmap (1 bit per 4KB page).
- **Static Page Cache**: A fixed array of 1024 slots to store indices of recently freed single pages. Makes grabbing one page super quick.
- **Free Page Counter**: A number (`free_pages`) that tracks how many pages are free. Saves time when checking if we can allocate.
- **Spinlock**: A lock to stop multiple CPU cores from messing with the bitmap or cache at the same time. We use it because cores can run at once, and without it, they’d step on each other’s toes, causing crashes or double-allocated pages.

## How It Works

### Starting Up (`pmm_init`)
When the kernel boots:
1. Grabs the memory map from the Limine bootloader, which lists what memory is usable (not reserved for hardware or firmware).
2. Figures out the highest memory address to size the bitmap and counts free pages in usable areas.
3. Picks a chunk of usable memory for the bitmap, sets it up, and marks all bits as used (`1`) to start safe.
4. Clears the static cache (sets all 1024 slots to 0) and sets its size.
5. Goes through usable memory again, marking those pages free (`0`) in the bitmap and updating the free page count.

**Why?** The bitmap needs to cover all possible pages, so we need the highest address. Starting with all bits as `1` avoids accidentally using reserved memory. The cache is static to avoid needing the PMM before it’s ready. We use Limine’s map because it’s reliable and tells us exactly what memory we can touch.

### Grabbing Pages (`pmm_request_pages`)
When something needs memory:
1. Checks if the number of pages asked for is legit (not 0, not more than what’s free).
2. Locks the spinlock so other cores wait.
3. If it’s just one page and the cache isn’t empty, grabs a page index from the cache, marks it used in the bitmap, drops the free page count, and hands back the address.
4. If the cache is empty or it’s multiple pages, scans the bitmap in 64-bit chunks (words). Skips full chunks (all `1`s) and looks for enough free pages in a row.
5. When it finds enough, marks them used, updates the free count, and returns the address (adds `hhdm_offset` if the caller wants a virtual address for the kernel’s higher-half mapping).
6. Unlocks the spinlock.

**Why?** The cache makes single-page grabs fast since scanning the bitmap is slower. Checking 64-bit words speeds up scanning by looking at 64 pages at once. The free page count lets us bail early if there’s not enough memory. The `hhdm_offset` thing supports virtual memory by giving addresses where the kernel lives.

### Freeing Pages (`pmm_release_pages`)
When memory’s done being used:
1. Makes sure the pointer’s valid, page-aligned, and not out of bounds.
2. Locks the spinlock.
3. Turns the pointer into a page index (subtracts `hhdm_offset` if it’s a virtual address).
4. For each page, checks if it’s used, then clears its bitmap bit, bumps the free page count, and if it’s a single page, tosses it into the cache if there’s room.
5. Unlocks the spinlock.

**Why?** Checking if pages are used prevents double-free bugs. The cache gets filled with single pages because those are grabbed most often. Validation stops bad pointers from breaking things. The spinlock keeps it safe across cores.

### Checking Free Memory (`pmm_get_free_pages`)
Just locks, reads the free page count, unlocks, and returns it.

**Why?** It’s a quick way to see how much memory’s left for debugging or system stats. The spinlock ensures the number’s accurate.

## Why This Way?
- **Bitmap**: It’s dead simple—one bit per page, easy to debug, and doesn’t waste much memory. Bit operations are fast, and scanning in 64-bit chunks makes it even quicker.
- **Static Cache**: Single-page allocations happen a lot (think kernel structs or small buffers). The cache skips bitmap scans, saving time. It’s static to avoid boot-time headaches where we’d need memory before the PMM is ready.
- **Free Page Counter**: Stops us from pointlessly scanning the bitmap when there’s not enough memory. Also handy for monitoring.
- **Spinlock**: Multi-core systems need it to avoid chaos. It’s lightweight for quick operations like these.

## Trade-Offs
- The cache only helps single pages. Multi-page allocations always scan the bitmap, but those are less common.
- Scanning the bitmap can be slow if memory’s fragmented or the system’s huge. The 64-bit word trick helps, but it’s still linear.
- The cache is fixed at 1024 entries. Might be too big for tiny systems or too small for massive ones, but it’s a solid middle ground.
- The spinlock could slow things down if tons of cores are allocating at once, but PMM stuff is fast, so it’s not a big deal.

## Why It’s Important
The PMM is the backbone of memory in the kernel. Everything—virtual memory, heaps, drivers—starts with physical pages. It’s gotta be fast so the kernel doesn’t lag, safe so it doesn’t crash, and lean so it doesn’t hog memory. This setup gets that done with a simple bitmap, a speedy cache, and enough checks to keep things solid.