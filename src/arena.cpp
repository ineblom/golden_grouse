
#define push_array_no_zero_aligned(a, T, c, align) (T *)arena_push((a), sizeof(T)*(c), (align))
#define push_array_aligned(a, T, c, align) (T *)memset(push_array_no_zero_aligned(a, T, c, align), 0, sizeof(T)*(c))
#define push_array_no_zero(a, T, c) push_array_no_zero_aligned(a, T, c, Max(8, __alignof(T)))
#define push_array(a, T, c) push_array_aligned(a, T, c, Max(8, __alignof(T)))

static Arena *arena_alloc(Arena_Params params) {
  u64 reserve_size = params.reserve_size;
  u64 commit_size = params.commit_size;

  if (params.flags & ARENA_FLAG__LARGE_PAGES) {
    reserve_size = AlignPow2(reserve_size, MiB(2));
    commit_size = AlignPow2(commit_size, MiB(2));
  } else {
    u64 page_size = getpagesize();
    reserve_size = AlignPow2(reserve_size, page_size);
    commit_size = AlignPow2(commit_size, page_size);
  }

  void *base = params.optional_backing_buffer;
  if (base == NULL) {
    /*MAP_HUGETLB*/
    base = mmap(0, reserve_size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    mprotect(base, commit_size, PROT_READ|PROT_WRITE);
  }

  Arena *arena = (Arena *)base;
  arena->current = arena;
  arena->flags = params.flags;
  arena->cmt_size = (u32)params.commit_size;
  arena->res_size = params.reserve_size;
  arena->base_pos = 0;
  arena->pos = ARENA_HEADER_SIZE;
  arena->cmt = commit_size;
  arena->res = reserve_size;

  return arena;
}

static void arena_release(Arena *arena) {
  for (Arena *n = arena, *prev = 0; n != 0; n = prev) {
    prev = n->prev;
    munmap(n, n->res);
  }
}

static void *arena_push(Arena *arena, u64 size, u64 align) {
  Arena *current = arena->current;
  u64 pos_pre = AlignPow2(current->pos, align);
  u64 pos_pst = pos_pre + size;

  if (pos_pst > current->res && !(arena->flags & ARENA_FLAG__NO_CHAIN)) {
    Arena *new_block = NULL;

    // freelist?

    if (new_block == NULL) {
      u64 res_size = current->res_size;
      u64 cmt_size = current->cmt_size;
      if (size + ARENA_HEADER_SIZE > res_size) {
        res_size = AlignPow2(size + ARENA_HEADER_SIZE, align);
        cmt_size = AlignPow2(size + ARENA_HEADER_SIZE, align);
      }
      new_block = arena_alloc({
        .flags = current->flags,
        .reserve_size = res_size,
        .commit_size = cmt_size,
      });
      new_block->base_pos = current->base_pos + current->res;
    }

    new_block->prev = arena->current;
    arena->current = new_block;

    current = new_block;
    pos_pre = AlignPow2(current->pos, align);
    pos_pst = pos_pre + size;
  }

  if (current->cmt < pos_pst) {
    u64 cmt_pst_aligned = pos_pst + current->cmt_size-1;
    cmt_pst_aligned -= cmt_pst_aligned%current->cmt_size;
    u64 cmt_pst_clamped = ClampTop(cmt_pst_aligned, current->res);
    u64 cmt_size = cmt_pst_clamped - current->cmt;
    u8 *cmt_ptr = (u8 *)current + current->cmt;

    mprotect(cmt_ptr, cmt_size, PROT_READ|PROT_WRITE); // alt. commit large (on non mac platforms)

    current->cmt = cmt_pst_clamped;
  }

  void *result = NULL;
  if (current->cmt >= pos_pst) {
    result = (u8 *)current + pos_pre;
    current->pos = pos_pst;
  }

  return result;
}

static u64 arena_pos(Arena *arena) {
  Arena *current = arena->current;
  u64 pos = current->base_pos + current->pos;
  return pos;
}

static void arena_pop_to(Arena *arena, u64 pos) {
  u64 big_pos = ClampBot(pos, ARENA_HEADER_SIZE);
  Arena *current = arena->current;

  for (Arena *prev = 0; current->base_pos >= big_pos; current = prev) {
    prev = current->prev;
    munmap(current, current->res);
  }

  arena->current = current;
  u64 new_pos = big_pos - current->base_pos;
  current->pos = new_pos;
}

static void arena_clear(Arena *arena) {
  arena_pop_to(arena, 0);
}

static void arena_pop(Arena *arena, u64 amt) {
  u64 old_pos = arena_pos(arena);
  u64 new_pos = old_pos;
  if (amt < old_pos) {
    new_pos = old_pos - amt;
  } else {
    new_pos = 0;
  }
  arena_pop_to(arena, new_pos);
}
