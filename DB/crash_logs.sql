create table public.crash_logs (
  id bigint generated always as identity not null,
  device_id text not null,
  firmware_version text null,
  reboot_reason text null,
  crashed_task text null,
  fault_pc text null,
  backtrace text null,
  uptime_before_crash bigint null,
  reported_at timestamp with time zone not null default now(),
  constraint crash_logs_pkey primary key (id)
) TABLESPACE pg_default;

create index IF not exists crash_logs_device_idx on public.crash_logs using btree (device_id) TABLESPACE pg_default;

create index IF not exists crash_logs_time_idx on public.crash_logs using btree (reported_at desc) TABLESPACE pg_default;