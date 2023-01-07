/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/lib/kernel/hash.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init(void) {
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */


}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
	page_get_type(struct page *page) {
	int ty = VM_TYPE(page->operations->type);
	switch (ty) {
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
	vm_initializer *init, void *aux) {

	ASSERT(VM_TYPE(type) != VM_UNINIT)

		struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check whether the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *init_page = vm_alloc_page()
			uninit_new(NULL, , init, type, aux, uninit_initialize(upage, NULL))
			/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
	spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page(struct supplemental_page_table *spt UNUSED,
	struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */

	return succ;
}

void
spt_remove_page(struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void) {
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void) {
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED) {
}

// exception.c에서 쓰임
/* Return true on success */
bool
vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
	bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */

	/* TODO: Your code goes here */

	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page(struct page *page) {
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page(void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page) {
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	return swap_in(page, frame->kva);
}

typedef bool va_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	// Returns true if A is less than B, or false if A is greater than or equal to B. 
		// hash_entry(HASH_ELEM, STRUCT, MEMBER) ((STRUCT *) ((uint8_t *) &(HASH_ELEM)->list_elem- offsetof (STRUCT, MEMBER.list_elem)))
	return hash_entry(a, struct supplemental_page_table_entry, elem)->va < hash_entry(b, struct supplemental_page_table_entry, elem)->va;
}

/* Computes and returns the hash value for hash element E, given
 * auxiliary data AUX. */
typedef uint64_t va_hash_func(const struct hash_elem *e, void *aux) {
	uint64_t va = hash_entry(e, struct supplemental_page_table_entry, elem)->va;
	return va & ~0xFFF;
}

// static bool
// thread_priority_greater(const struct list_elem *lhs, const struct list_elem *rhs, void *aux UNUSED) {
// 	return list_entry(lhs, struct thread, elem)->priority > list_entry(rhs, struct thread, elem)->priority;

/* Initialize new supplemental page table */
void
supplemental_page_table_init(struct supplemental_page_table *spt UNUSED) {
	hash_init(spt->spt, va_hash_func, va_less_func, NULL);

}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
	struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}