import { Component, OnDestroy, OnInit } from '@angular/core';
import { HttpErrorResponse } from '@angular/common/http';
import { Subject, catchError, of, switchMap, takeUntil, timer } from 'rxjs';
import { ToastrService } from 'ngx-toastr';
import { EthernetStatus as IEthernetStatus } from 'src/app/generated/models';
import { SystemApiService } from 'src/app/services/system.service';
import { LoadingService } from 'src/app/services/loading.service';
import { getHttpErrorMessage } from 'src/app/utils/error-handler';

type TableRow = {
  label: string;
  value: string;
  valueClass?: string;
};

const POLL_INTERVAL_MS = 3000;

@Component({
  selector: 'app-ethernet',
  templateUrl: './ethernet.component.html',
  standalone: false
})
export class EthernetComponent implements OnInit, OnDestroy {
  public status: IEthernetStatus | null = null;
  public rows: TableRow[] = [];
  public unreachable = false;

  // Value of the "enable" checkbox. Initialised from the device once and then left
  // alone so polling does not overwrite what the user is editing.
  public enabledSetting = false;
  public settingTouched = false;

  private settingInitialised = false;
  private destroy$ = new Subject<void>();

  constructor(
    private systemService: SystemApiService,
    private loadingService: LoadingService,
    private toastr: ToastrService,
  ) {}

  ngOnInit(): void {
    timer(0, POLL_INTERVAL_MS).pipe(
      switchMap(() => this.systemService.getEthernetStatus().pipe(
        catchError(() => of(null))
      )),
      takeUntil(this.destroy$)
    ).subscribe(status => {
      this.unreachable = status === null;
      if (status === null) {
        return;
      }
      this.status = status;
      if (!this.settingInitialised) {
        this.enabledSetting = status.enabled;
        this.settingInitialised = true;
      }
      this.rows = this.buildRows(status);
    });
  }

  ngOnDestroy(): void {
    this.destroy$.next();
    this.destroy$.complete();
  }

  public onSettingChange(): void {
    this.settingTouched = true;
  }

  public saveSetting(): void {
    this.systemService.updateSystem('', { ethernetEnabled: this.enabledSetting ? 1 : 0 })
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => {
          this.settingTouched = false;
          this.toastr.success('Saved Ethernet setting');
          this.toastr.warning('Restart the device for the change to take effect.');
        },
        error: (err: HttpErrorResponse) => {
          this.toastr.error(`Could not save Ethernet setting. ${getHttpErrorMessage(err)}`);
        }
      });
  }

  public restartDevice(): void {
    this.systemService.restart()
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe({
        next: () => this.toastr.success('Device restarted'),
        error: (err: HttpErrorResponse) => {
          this.toastr.error(`Could not restart device. ${getHttpErrorMessage(err)}`);
        }
      });
  }

  public trackByLabel(_: number, row: TableRow): string {
    return row.label;
  }

  private buildRows(s: IEthernetStatus): TableRow[] {
    const rows: TableRow[] = [];

    rows.push({
      label: 'Ethernet',
      value: s.enabled ? (s.active ? 'Enabled' : 'Enabled (not running)') : (s.active ? 'Disabled (still running until restart)' : 'Disabled'),
    });

    if (s.error) {
      rows.push({ label: 'Error', value: `Module not started (${s.error})`, valueClass: 'text-red-500' });
    }

    if (!s.active) {
      rows.push({ label: 'Link', value: 'Not running' });
    } else {
      const linkDetails = s.linkUp
        ? `Up${s.speedMbps ? `, ${s.speedMbps} Mbit/s` : ''}${s.speedMbps ? (s.fullDuplex ? ', full duplex' : ', half duplex') : ''}`
        : 'Down (no cable or no link)';
      rows.push({ label: 'Link', value: linkDetails });
    }

    rows.push({ label: 'LAN status', value: s.connected ? 'Connected' : 'Disconnected' });
    rows.push({ label: 'DHCP', value: s.dhcp ? 'Active' : 'Inactive' });
    rows.push({ label: 'IPv4 address', value: s.ipv4 || '-' });
    rows.push({ label: 'Subnet mask', value: s.netmask || '-' });
    rows.push({ label: 'Gateway', value: s.gateway || '-' });
    rows.push({ label: 'MAC address', value: s.mac || '-' });

    rows.push({ label: 'Wi-Fi', value: s.wifiConnected ? 'Connected' : 'Disconnected' });
    rows.push({ label: 'Wi-Fi status', value: s.wifiStatus || '-' });

    const activeNames: Record<string, string> = { ethernet: 'Ethernet (LAN)', wifi: 'Wi-Fi', none: 'None' };
    rows.push({ label: 'Active connection', value: activeNames[s.activeInterface] ?? s.activeInterface });

    return rows;
  }
}
