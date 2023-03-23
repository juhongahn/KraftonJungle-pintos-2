# pintos project
자세한 내용은 juhong branch를 참조하시면 됩니다.

## Trouble Shooting

### load_segment에서 offset을 갱신 해야하는 문제.

프로젝트3 이전에서, load_segment는 load하고자 하는 실행 파일을 페이지 단위로 쪼개지 않고 물리메모리에 바로 매핑했다. 하지만 프로젝트3에서는 lazy load 방식을 사용함으로써 페이지 단위로 쪼개고, 페이지 폴트가 발생하고 나서야 메모리에 매핑하는 방식을 사용한다. 때문에 lazy_load_segment에서는 폴트가 발생했을 때 로드할 파일의 정보를 담고있는 구조체가 필요했고 load_info 라는 구조체를 만들어서 폴트 발생시 같이 전달해 주었다.

그럼에도 문제는 해결되지 않았다. 문제의 원인은 offset에 있었다. 스켈레톤 코드에서는 offset을 갱신해주는 코드가 없었고 lazy load가 일어났을 때는 매번 파일의 같은 위치를 읽어 매핑하는것이 문제였다. 때문에 읽은 바이트 만큼 offset을 갱신해주어야 했다.

```c
bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
	uint32_t read_bytes, uint32_t zero_bytes, bool writable) {

	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		/* TODO: Set up aux to pass information to the lazy_load_segment. */

		struct load_info *mem_init_info = (struct load_info *)calloc(1, sizeof(struct load_info));
		mem_init_info->file = file;
		mem_init_info->ofs = ofs;
		
		mem_init_info->page_read_bytes = page_read_bytes;
		mem_init_info->page_zero_bytes = page_zero_bytes;

		void *aux = mem_init_info;
		if (!vm_alloc_page_with_initializer(VM_ANON, upage,
			writable, lazy_load_segment, aux))
			return false;
		
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;

		/* 오프셋을 옮겨보자 */
		ofs += page_read_bytes;
	}
	return true;
}
```

- VM_TYPE으로 stack은 어떻게 처리할지?
  - uninit / anonymous / file_backed 세가지 종류만 있다는데 anon의 한 종류인 stack을 어떻게 처리할 것인가?
    - VM_ANON | VM_MARKER_0 사용하면 됨. 일단 VM_STACK을 따로 만들었었는데, 필요하다면 리팩토링할 계획.

깃북에서 setup_stack 메서드를 구현할 때 ‘Map the stack on stack_bottom and claim the page immediately’ 즉 stack bottom에 페이지를 만들고 곧바로 claim 하라고 한다. 우리팀은 이를 lazy_load 과정 없이 물리 메모리에 매핑해야한다고 해석했기에  vm_alloc_page을 호출해 vm_alloc_page_with_initializer에서 vm_initializer의 인자로 null값을 넣어줬다. 이후 곧바로 vm_claim_page를 호출, 물리메모리에 매핑해주었다. 깃북은 vm_alloc_page을 호출할 때 타입으로 마커를 사용해도 좋다고 했다. 우리팀은 VM_STACK이라는 새로운 타입을 만들어서 스택 페이지를 할당할 때 사용했고 vm_do_claim_page 메서드에서 swap_in 루틴이 실행되지 못하도록 구현했다. swap_in 루틴은 vm_alloc_page_with_initializer에서 인자로 준 page_initializer와 vm_initializer로 들어온 메서드를 실행시켜주는데 우리 코드의 스택의 경우 이 둘값이 null이기 때문이다.

```c
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT);
	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *initial_page = (struct page *)calloc(1, sizeof(struct page));
		typedef bool(*page_initializer)(struct page *, enum vm_type, void *);
		page_initializer initializer = NULL;

		switch (type)
		{
			case VM_ANON:
				initializer = anon_initializer;
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
		}
		
		uninit_new(initial_page, upage, init, type, aux, initializer);
		initial_page->writable = writable;
		if (type == VM_STACK)
			initial_page->stack = true;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, initial_page);
	}

err:
	return false;
}
```

```c
/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	ASSERT (page != NULL);

	struct frame *frame = vm_get_frame();
	struct thread * t_curr = thread_current();

	/* Set links */
	ASSERT (frame != NULL);
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* 스택의 경우 initializer가 없다. */
	if (pml4_get_page(t_curr->pml4, page->va) == NULL 
		&& pml4_set_page(t_curr->pml4, page->va, frame->kva, page->writable))
	{
		if (page->stack == true)
		{
			return true;
		}
		
		return swap_in (page, frame->kva);	// 여기서 uninit_initialize 호출된다.

	} else {
		printf("vm_do_claim_page: mapping fail\n");
	}

	return false;
}
```


### stack growth에서 rsp로 어떤 값을 사용할지
  - 정확한 이해가 더 필요한 상황
  
### system_call 호출시 유효하지 않은 address처리

```c
void
check_address(void *addr) {
	/* project3 modification 
	*  lazy_load_segment로 pml4_get_page NULL이 나오는것은 자연스러운 것.
	*/

	/* 1. 포인터 유효성 검증
	 *  <유효하지 않은 포인터>
	 *  - 널 포인터
	 *  - virtual memory와 매핑 안 된 영역
	 *  - 커널 가상 메모리 주소 공간을 가리키는 포인터 (=PHYS_BASE 위의 영역)
	 * 2. 유저 영역을 벗어난 영역일 경우 프로세스 종료((exit(-1)))
	 */
	struct thread *t_curr = thread_current();
	if (addr == NULL || is_kernel_vaddr(addr) || pml4_get_page(t_curr->pml4, addr) == NULL) {
		exit(-1);
	}
}
```

프로젝트 2에서 유효한 주소인지 판별하는 메서드 check_address는 페이지 테이블을 참조해 해당 주소에 해당하는 페이지가 물리메모리에 매핑돼 있지 않다면 버그로 인식, exit(-1) 시켜 버렸다. 하지만 lazy load에서는 페이지 폴트가 버그가 아니기에 물리 메모리에 페이지에 해당하는 주소가 매핑돼 있지 않은것은 자연스러운 일이다. 때문에 프로그램을 종료시키지말고 페이지_폴트_핸들러가 루틴을 진행하도록 해야한다.

```c
void
check_address(void *addr) {
	/* project3 modification 
	*  lazy_load_segment로 pml4_get_page NULL이 나오는것은 자연스러운 것.
	*/

	/* 1. 포인터 유효성 검증
	 *  <유효하지 않은 포인터>
	 *  - 널 포인터
	 *  - virtual memory와 매핑 안 된 영역
	 *  - 커널 가상 메모리 주소 공간을 가리키는 포인터 (=PHYS_BASE 위의 영역)
	 * 2. 유저 영역을 벗어난 영역일 경우 프로세스 종료((exit(-1)))
	 */
	struct thread *t_curr = thread_current();
	if (addr == NULL || is_kernel_vaddr(addr)) {
		exit(-1);
	}

	if (pml4_get_page(t_curr->pml4, addr) == NULL) {
		if (!spt_find_page(&t_curr->spt, addr))
			exit(-1);
	}
}
```


- supplemental page table의  kill/ copy 할 때 겪었던 문제들
  - supplemental_page_table_kill => hash_destroy 메서드는 테이블 까지 지워버림. 때문에 init하고도 해쉬 테이블이 없어서 문제가 발생.

process_exec는 load를 호출하기전에 process_cleanup을 호출한다. 이 때 supplemental_page_table_kill 메서드를 호출하는데, 문제는 supplemental_page_table_kill 구현 방식이었다. spt로 hash를 썼기에 hash value들을 청소해주면 되겠지 하는 마음에 hash_clear과 버킷까지 해제하는 'hash_destroy'를 사용했었다. 때문에 기껏 initd에서 supplemental_page_table_init로 초기화 해둔 해쉬 테이블을 날려버리니 load과정에서 page를 테이블에 넣으려하니 문제가 발생했다. 때문에 hash테이블을 해제하는 hash_destroy가 아니라 페이지만 날리는 hash_clear로 변경해주었다.

```c
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	if (spt->hash_page_table) {
		hash_clear(spt->hash_page_table, page_destructor);
	}
	else
		return;

}
```


## Skeleton Code Understanding
- 다음 용어들 사이에서의 혼란
  1. pml4로 관리하는 page table
  2. spt의 역할
  3. page 구조체
  4. frame 구조체와 실제 physical memory의 연결
    
    spt가 기존의 plm4를 대체하는 page table인가?라는 생각으로 한 동안 혼란에 빠졌었다. spt는 단어 그대로 supplement의 역할이었고, page fault가 발생해서 가상페이지를 찾을 때, 프로세스가 종료될 때 자원을 반환해줄 때 사용된다.
page, frame 두 구조체는 모두 자료형일 뿐이므로 관리하는 용도로 사용하려고 만든게 당연한 것이었다. (page구조체가 page자체를 나타내는 것이 아니며 frame 구조체가 frame 자체를 나타내는 것이 아니라는 뜻)
    그런데 이렇게 각 페이지들마다 page구조체/frame구조체를 가지고 관리는 비용은 어떻게 될까? 우선, page 구조체는 가상 메모리가 아니라 물리 메모리를 관리하기 위해 존재한다. page 구조체는 물리 메모리의 모든 page마다 하나씩 할당된다.
    물리 메모리가 4GB, page 크기가 4KB라면, 4GB/4KB = 2^20개의 page 구조체 인스턴스가 생성된다. 즉, page 구조체의 크기가 40Byte인 경우 40MB를 사용하게 된다. 커널은 이 page 구조체 인스턴스를 통해, 어떤 물리 메모리 영역이 free인지, 혹은 어떤 용도로 할당되어있는지를 파악할 수 있다. 실제 리눅스에서 어떻게 구현되었는지는 [여기](https://elixir.bootlin.com/linux/v4.6/source/include/linux/mm_types.h#L44)에서 확인할 수 있다.

    --- 
- `uninit_new` 함수와 `uninit_initialize` 함수
  1. `uninit_new`는 page 구조체에 미래에 어떤 page로 사용될 지 기록하는 역할이다.
  2. `uninit_initialize`는 page 구조체에 기록된 정보를 바탕으로 page의 속성을 실제로 변화 시킨다. 
  
  이렇게 두 함수로 나누어 임시 매핑을 만드는 시점과 페이지를 실제로 할당해야 할 시점을 따로 잡아 놓으면, 즉 유저 프로세스가 특정 주소에 접근하기 전까지는 페이지 할당을 실제로 하지 않고 매핑이 존재한다는 정보만 page 구조체를 통해 남겨 놓으면 시간/공간 상의 이점을 챙길 수 있다.
  임시 매핑을 만드는 `vm_alloc_page_with_initialize` 함수에서 `uninit_new`를 실행하며, 이 시점에 페이지의 실제 물리 메모리 할당과 매핑은 되지 않는다. 이때 spt에 insert하여 **임시 매핑**은 존재하지만, pml4 상으로는 valid하지 않은 주소이므로, 후에 해당 주소로 접근할 때는 page fault가 발생하고, 이 때 page구조체 내의 swap_in이 호출되며, 아직까진 uninit_page이므로 [uninit_page의 uninit_ops의 swap_in 역할]인 `uninit_initialize`가 실행된다.
  `uninit_initialize`가 실행되면서, `uninit_new`의 인자인 `init`으로 전달했던 함수와(예를들면 `lazy_load_segment`) 인자로 전달했던 type에 맞는 initializer(`anon_initializer`, `file_backed_initializer`)가 실행되어 실제 물리 메모리의 할당과 매핑이 모두 이루어지게 된다.

![image](https://user-images.githubusercontent.com/115034667/212542247-37bb125d-fce0-4f06-bda5-d5b1a1bc52fa.png)
[그림출처](https://img1.daumcdn.net/thumb/R1280x0/?scode=mtistory2&fname=https%3A%2F%2Fblog.kakaocdn.net%2Fdn%2FbdPtWH%2FbtrtZamOQez%2F5GUEAf9dh5QQpb4dkDfJrK%2Fimg.png)

#### 읽어보면 좋을 링크들

[Linux Kernel : struct page 구조체](https://showx123.tistory.com/70)
[[Linux] pageflags로 살펴본 메모리의 일생](http://egloos.zum.com/studyfoss/v/5512112)


---
## mmap Trouble Shooting 1

### syscall write에서 buffer의 writable?
```C
static void
check_buffer(void* buffer, unsigned size, bool writable) {
	struct page* page = check_address(buffer + size);
	if(page == NULL)
		exit(-1);
	if(!writable && page->writable == false)
		exit(-1);
}
```
read 함수에서와 마찬가지로 무지성으로
```C
if (!page->writable)
    exit(-1);
```
와 같이 buffer를 포함하는 페이지의 writable을 체크하는 조건문을 넣어줬다가 영문도 모른채 mmap-clean test에서 계속 fail이 나왔다.
그래서 관련하여 write 함수에서 buffer를 담고 있는 page의 writable 여부를 체크해줘야하는가?하는 생각을 하다가, 다른 사람들은 어떻게 처리를 했는지 둘어보던 와중, 이상한 조건으로 체크를 하는 것을 발견했다.
```C
if(!writable && page->writable == false)
```
에서 writable의 값을 하드코딩으로 넣어주었는데, write 함수의 경우 writable을 1로 넣어주어 아예 조건문이 의미가 없었다.

그러면 결국 page->writable과 관련을 짓지 않아도 된다는 뜻인데, 이는 fd로 open한 파일에 buffer의 값을 넣는 함수가 write 함수이기 때문에 매우 당연한 것이었다. 즉 고민할거리도 아니었다.

### 그렇다면 syscall read에서는?
반대로 read는 불러온 파일의 내용을 buffer에 쓰는 과정이기 때문에 buffer를 포함하는 page의 writable 여부를 확인해줘야한다.
```C
if (!page->writable)
    exit(-1);
```

## Trouble Shooting 2
### spt_find_page에서 addr로 page가 찾아지지 않는다. (임시해결)

일단 다른 사람들의 코드는 정상적으로 작동하는데, 우리 코드의 경우 do_munmap() 함수에서 에러가 발생했다.

기존의 방식은 spt_find_page()함수를 이용해서 page를 찾고, 해당 페이지에 dirty_bit를 확인해서 file_write()을 실행했다.

그런데 이때 spt_find_page()함수를 통해 page를 찾았을 때, return 으로 null값이 나와서 비정상종료가 되었다.

이 함수가 호출되는 경로인 supplemental_page_table_kill()함수 내의 hash_clear() 에서 hash_elem을 이용해 hash_entry() 매크로를 이용해서 munmap()함수를 호출하는 것으로 해결했다. 다른 함수에서는 같은 spt_find_page()가 정상적으로 작동하는데, munmap() 내부에서는 작동하지 않았다.(상위 콜러 몇개에서도 안됨, hash table에 페이지가 들어있는 것은 확인)

일단 test 통과를 위해서 임의로 다른 방식으로 page를 찾도록 구성했는데, 왜 spt_find_page()를 찾는게 불가능했다. 이유는 여전히 불명이어서 추후 탐구해볼 예정이다.



### destroy 함수 활용?
스켈레톤 코드들이 괜히 있는 코드가 아닐텐데, 
destruct -> vm_dealloc_page -> destory로 이어지는데, destroy를 어떻게 활용할지 궁금증 해소 안됨
vm_dealloc_page에서 호출하는 destroy를 활용하고 싶은데, 
VM type마다 각각의 서로 다른 destroy(page)가 호출이 되도록 짜여져 있는데 각각의 페이지의 타입별로 어떤 조치를 취해줘야 할지 고민하다 끝남.
- mmap의 경우 munmap이 불린 것도 아닌데 페이지가 중간에 하나가 삭제된다면 destroy에서 어떻게 자원(물리 메모리) 반환 처리를 해줘야할지 등


swap in / swap out에서도 안하고, table kill 할 때만 destory가 실행되는데, 다들 process exit되는 상황 외에는 메모리의 명시적 해제, 즉 palloc_free_page를 딱히 안하고 있어서 이상하다. (굳이 구현하란 말이 없어서 생각안해도 되는 것인지)

- gitbook 참고-  `A page can have a life cycle of initialize->(page_fault->lazy-load->swap-in>swap-out->...)->destroy. `

프로세스 exit 될 때 외에는 palloc으로 할당한 메모리의 반환을 어디서 해야할지 모르겠다.

```C
static void
file_backed_destroy(struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	if (page->frame->kva != NULL)
		palloc_free_page(page->frame->kva);
}
```
이런식으로라도 작성해두어야 하는 것 아닌지?

# juhongahn-KraftonJungle-pintos-2
