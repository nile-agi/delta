<script lang="ts">
	import { onMount } from 'svelte';
	import { Plus, Check, Trash2 } from '@lucide/svelte';
	import Button from '$lib/components/ui/button/button.svelte';
	import * as Dialog from '$lib/components/ui/dialog';
	import * as Select from '$lib/components/ui/select';
	import Input from '$lib/components/ui/input/input.svelte';
	import Textarea from '$lib/components/ui/textarea/textarea.svelte';
	import Label from '$lib/components/ui/label/label.svelte';
	import Badge from '$lib/components/ui/badge/badge.svelte';
	import Checkbox from '$lib/components/ui/checkbox/checkbox.svelte';
	import {
		taskList,
		tasksLoading,
		loadTasks,
		createTask,
		completeTask,
		deleteTask,
		setFilterStatus,
		setFilterPriority,
		tasksFilterStatus,
		tasksFilterPriority
	} from '$lib/stores/tasks.svelte';

	let showCreateDialog = $state(false);
	let newTask = $state({ title: '', description: '', priority: 'medium', due_date: '', tags: '' });

	const tasks = $derived(taskList());
	const loading = $derived(tasksLoading());
	const filterStatus = $derived(tasksFilterStatus());
	const filterPriority = $derived(tasksFilterPriority());

	const pendingTasks = $derived(tasks.filter((t) => t.status === 'pending' || t.status === 'in_progress'));
	const completedTasks = $derived(tasks.filter((t) => t.status === 'completed'));

	function priorityColor(priority: string): string {
		switch (priority) {
			case 'urgent':
				return 'bg-red-500/10 text-red-500 border-red-500/20';
			case 'high':
				return 'bg-orange-500/10 text-orange-500 border-orange-500/20';
			case 'medium':
				return 'bg-blue-500/10 text-blue-500 border-blue-500/20';
			case 'low':
				return 'bg-gray-500/10 text-gray-500 border-gray-500/20';
			default:
				return '';
		}
	}

	async function handleCreateTask() {
		if (!newTask.title) return;
		await createTask(newTask);
		newTask = { title: '', description: '', priority: 'medium', due_date: '', tags: '' };
		showCreateDialog = false;
	}

	async function handleComplete(id: string) {
		await completeTask(id);
	}

	async function handleDelete(id: string) {
		await deleteTask(id);
	}

	function handleFilterStatus(value: string) {
		setFilterStatus(value === 'all' ? '' : value);
		loadTasks(value === 'all' ? undefined : value, filterPriority || undefined);
	}

	function handleFilterPriority(value: string) {
		setFilterPriority(value === 'all' ? '' : value);
		loadTasks(filterStatus || undefined, value === 'all' ? undefined : value);
	}

	onMount(() => {
		loadTasks();
	});
</script>

<div class="flex h-full flex-col">
	<div class="flex items-center justify-between border-b px-6 py-4">
		<div class="flex items-center gap-4">
			<h1 class="text-2xl font-semibold">Tasks</h1>
			<div class="flex items-center gap-2">
				<Select.Root
					type="single"
					value={filterStatus || 'all'}
					onValueChange={handleFilterStatus}
				>
					<Select.Trigger class="h-8 w-[130px] text-xs">
						{filterStatus || 'All statuses'}
					</Select.Trigger>
					<Select.Content>
						<Select.Item value="all">All statuses</Select.Item>
						<Select.Item value="pending">Pending</Select.Item>
						<Select.Item value="in_progress">In Progress</Select.Item>
						<Select.Item value="completed">Completed</Select.Item>
					</Select.Content>
				</Select.Root>
				<Select.Root
					type="single"
					value={filterPriority || 'all'}
					onValueChange={handleFilterPriority}
				>
					<Select.Trigger class="h-8 w-[130px] text-xs">
						{filterPriority || 'All priorities'}
					</Select.Trigger>
					<Select.Content>
						<Select.Item value="all">All priorities</Select.Item>
						<Select.Item value="urgent">Urgent</Select.Item>
						<Select.Item value="high">High</Select.Item>
						<Select.Item value="medium">Medium</Select.Item>
						<Select.Item value="low">Low</Select.Item>
					</Select.Content>
				</Select.Root>
			</div>
		</div>
		<Button size="sm" onclick={() => (showCreateDialog = true)}>
			<Plus class="mr-1 h-4 w-4" />
			New Task
		</Button>
	</div>

	<div class="flex-1 overflow-auto p-6">
		{#if loading}
			<div class="flex items-center justify-center py-12">
				<p class="text-muted-foreground">Loading tasks...</p>
			</div>
		{:else}
			{#if pendingTasks.length > 0}
				<div class="mb-6">
					<h2 class="mb-3 text-sm font-semibold text-muted-foreground">
						Active ({pendingTasks.length})
					</h2>
					<div class="space-y-2">
						{#each pendingTasks as task}
							<div
								class="flex items-start gap-3 rounded-lg border p-4 transition-colors hover:bg-accent/50"
							>
								<button class="mt-0.5 shrink-0" onclick={() => handleComplete(task.id)}>
									<div
										class="flex h-5 w-5 items-center justify-center rounded border-2 border-muted-foreground/30 transition-colors hover:border-primary hover:bg-primary/10"
									>
									</div>
								</button>
								<div class="min-w-0 flex-1">
									<div class="flex items-center gap-2">
										<p class="text-sm font-medium">{task.title}</p>
										<Badge variant="outline" class={priorityColor(task.priority)}>
											{task.priority}
										</Badge>
									</div>
									{#if task.description}
										<p class="mt-1 text-xs text-muted-foreground">{task.description}</p>
									{/if}
									{#if task.due_date}
										<p class="mt-1 text-xs text-muted-foreground">
											Due: {new Date(task.due_date).toLocaleDateString()}
										</p>
									{/if}
									{#if task.tags}
										<div class="mt-1 flex gap-1">
											{#each task.tags.split(',').filter(Boolean) as tag}
												<Badge variant="secondary" class="text-[10px]">{tag.trim()}</Badge>
											{/each}
										</div>
									{/if}
								</div>
								<Button
									variant="ghost"
									size="icon"
									class="h-7 w-7 text-muted-foreground hover:text-destructive"
									onclick={() => handleDelete(task.id)}
								>
									<Trash2 class="h-3.5 w-3.5" />
								</Button>
							</div>
						{/each}
					</div>
				</div>
			{/if}

			{#if completedTasks.length > 0}
				<div>
					<h2 class="mb-3 text-sm font-semibold text-muted-foreground">
						Completed ({completedTasks.length})
					</h2>
					<div class="space-y-2">
						{#each completedTasks as task}
							<div class="flex items-start gap-3 rounded-lg border p-4 opacity-60">
								<div class="mt-0.5 shrink-0">
									<div
										class="flex h-5 w-5 items-center justify-center rounded border-2 border-primary bg-primary"
									>
										<Check class="h-3 w-3 text-primary-foreground" />
									</div>
								</div>
								<div class="min-w-0 flex-1">
									<p class="text-sm font-medium line-through">{task.title}</p>
								</div>
								<Button
									variant="ghost"
									size="icon"
									class="h-7 w-7 text-muted-foreground hover:text-destructive"
									onclick={() => handleDelete(task.id)}
								>
									<Trash2 class="h-3.5 w-3.5" />
								</Button>
							</div>
						{/each}
					</div>
				</div>
			{/if}

			{#if pendingTasks.length === 0 && completedTasks.length === 0}
				<div class="flex flex-col items-center justify-center py-12">
					<p class="mb-2 text-muted-foreground">No tasks yet</p>
					<Button variant="outline" size="sm" onclick={() => (showCreateDialog = true)}>
						<Plus class="mr-1 h-4 w-4" />
						Create your first task
					</Button>
				</div>
			{/if}
		{/if}
	</div>
</div>

<Dialog.Root bind:open={showCreateDialog}>
	<Dialog.Content class="sm:max-w-md">
		<Dialog.Header>
			<Dialog.Title>New Task</Dialog.Title>
		</Dialog.Header>
		<div class="space-y-4 py-4">
			<div>
				<Label>Title</Label>
				<Input bind:value={newTask.title} placeholder="Task title" class="mt-1" />
			</div>
			<div>
				<Label>Description</Label>
				<Textarea
					bind:value={newTask.description}
					placeholder="Description (optional)"
					class="mt-1"
					rows={3}
				/>
			</div>
			<div class="grid grid-cols-2 gap-3">
				<div>
					<Label>Priority</Label>
					<Select.Root type="single" value={newTask.priority} onValueChange={(v) => (newTask.priority = v)}>
						<Select.Trigger class="mt-1">{newTask.priority}</Select.Trigger>
						<Select.Content>
							<Select.Item value="low">Low</Select.Item>
							<Select.Item value="medium">Medium</Select.Item>
							<Select.Item value="high">High</Select.Item>
							<Select.Item value="urgent">Urgent</Select.Item>
						</Select.Content>
					</Select.Root>
				</div>
				<div>
					<Label>Due date</Label>
					<Input type="date" bind:value={newTask.due_date} class="mt-1" />
				</div>
			</div>
			<div>
				<Label>Tags</Label>
				<Input
					bind:value={newTask.tags}
					placeholder="work, personal (comma-separated)"
					class="mt-1"
				/>
			</div>
		</div>
		<Dialog.Footer>
			<Button variant="outline" onclick={() => (showCreateDialog = false)}>Cancel</Button>
			<Button onclick={handleCreateTask}>Create</Button>
		</Dialog.Footer>
	</Dialog.Content>
</Dialog.Root>
