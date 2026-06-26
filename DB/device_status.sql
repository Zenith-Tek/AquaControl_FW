create table public.device_status (
  device_id text not null,
  last_seen timestamp with time zone not null default now(),
  wifi_rssi integer null,
  uptime_seconds bigint null,
  firmware_version text null,
  last_reboot_reason text null,
  device_timestamp bigint null,
  constraint device_status_pkey primary key (device_id)
) TABLESPACE pg_default;

-- Enable Row Level Security
alter table public.device_status enable row level security;

-- Create Policies to allow public/anonymous access to upsert status
create policy "Allow insert for anon" on public.device_status
  for insert to anon with check (true);

create policy "Allow update for anon" on public.device_status
  for update to anon using (true);

create policy "Allow select for anon" on public.device_status
  for select to anon using (true);

-- Grant privileges to roles
grant all on table public.device_status to anon;
grant all on table public.device_status to authenticated;
grant all on table public.device_status to service_role;